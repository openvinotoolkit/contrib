# Copyright (C) 2018-2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

from transformers.file_utils import is_torch_available

if is_torch_available():
    import torch
    from transformers import MBartForConditionalGeneration, AutoConfig
    from transformers.modeling_outputs import BaseModelOutput, Seq2SeqLMOutput
    from transformers.generation_utils import GenerationMixin
else:

    class GenerationMixin(object):
        def __init__(self):
            pass


from .modeling_ov_utils import (
    OVPreTrainedModel,
    load_ov_model_from_pytorch,
    is_openvino_api_2,
)

from . import modeling_ov_utils


def _prepare_nlp_inputs(
    self,
    input_ids=None,
    attention_mask=None,
    decoder_input_ids=None,
    decoder_attention_mask=None,
    head_mask=None,
    decoder_head_mask=None,
    cross_attn_head_mask=None,
    encoder_outputs=None,
    past_key_values=None,
    inputs_embeds=None,
    decoder_inputs_embeds=None,
    labels=None,
    use_cache=None,
    output_attentions=None,
    output_hidden_states=None,
    return_dict=None,
):
    inputs = {
        "attention_mask": attention_mask,
        "decoder_input_ids": decoder_input_ids,
    }

    if past_key_values is not None:
        for i in range(12):
            for j in range(4):
                inputs[f"past_key_values.{i}.{j}"] = past_key_values[i][j]
    else:
        inputs["encoder_outputs"] = encoder_outputs.last_hidden_state

    return inputs


# A wrapper for MBart model. When use_cache=True is specified,
# first model run accepts only three inputs but next calls uses past_key_values.
# We cannot keep a single model for both cases because the graph is dynamic and connections are different.
class OVMBartForConditionalGeneration(GenerationMixin):
    def __init__(self, config, encoder, model, model_past=None):
        super().__init__()
        self.encoder = encoder
        self.model = model
        self.model_past = model_past

        origin_forward = self.encoder.forward

        def forward_wrap(self, *args, **kwargs):
            kwargs["return_dict"] = False
            res = origin_forward(*args, **kwargs)
            return BaseModelOutput(last_hidden_state=torch.tensor(res[0]))

        self.encoder.forward = lambda *args, **kwargs: forward_wrap(self.encoder, *args, **kwargs)

        self.model._prepare_nlp_inputs = lambda *args, **kwargs: _prepare_nlp_inputs(self.model, *args, **kwargs)
        if self.model_past is not None:
            self.model_past._prepare_nlp_inputs = lambda *args, **kwargs: _prepare_nlp_inputs(
                self.model_past, *args, **kwargs
            )

        self.config = config
        if is_torch_available():
            self.device = torch.device("cpu")

    def get_encoder(self):
        return self.encoder

    # Copied from MBartForConditionalGeneration
    def prepare_inputs_for_generation(
        self,
        decoder_input_ids,
        past=None,
        attention_mask=None,
        head_mask=None,
        decoder_head_mask=None,
        cross_attn_head_mask=None,
        use_cache=None,
        encoder_outputs=None,
        **kwargs,
    ):
        # cut decoder_input_ids if past is used
        if past is not None:
            decoder_input_ids = decoder_input_ids[:, -1:]

        return {
            "input_ids": None,  # encoder_outputs is defined. input_ids not needed
            "encoder_outputs": encoder_outputs,
            "past_key_values": past,
            "decoder_input_ids": decoder_input_ids,
            "attention_mask": attention_mask,
            "head_mask": head_mask,
            "decoder_head_mask": decoder_head_mask,
            "cross_attn_head_mask": cross_attn_head_mask,
            "use_cache": use_cache,  # change this to avoid caching (presumably for debugging)
        }

    # Copied from MBartForConditionalGeneration
    @staticmethod
    def _reorder_cache(past, beam_idx):
        reordered_past = ()
        for layer_past in past:
            # cached cross_attention states don't have to be reordered -> they are always the same
            reordered_past += (
                tuple(past_state.index_select(0, beam_idx) for past_state in layer_past[:2]) + layer_past[2:],
            )
        return reordered_past

    def __call__(self, *args, **kwargs):
        if kwargs.get("past_key_values", None):
            return self.model_past(*args, **kwargs)
        else:
            return self.model(*args, **kwargs)

    @classmethod
    def from_pretrained(cls, model_name_or_path, *model_args, **kwargs):
        from_pt = kwargs.pop("from_pt", False)
        use_cache = kwargs.get("from_pt", True)

        from_ov = not from_pt
        if from_ov:
            if not use_cache:
                raise NotImplementedError("Loading from IR supports only use_cache=True")
            config = kwargs.get("config") if "config" in kwargs else AutoConfig.from_pretrained(model_name_or_path)

            modeling_ov_utils.OV_WEIGHTS_NAME = "encoder.xml"
            encoder = OVPreTrainedModel.from_pretrained(model_name_or_path, *model_args, **kwargs)

            modeling_ov_utils.OV_WEIGHTS_NAME = "first_run.xml"
            net = OVPreTrainedModel.from_pretrained(model_name_or_path, *model_args, **kwargs)

            modeling_ov_utils.OV_WEIGHTS_NAME = "ov_model.xml"
            net_past = OVPreTrainedModel.from_pretrained(model_name_or_path, *model_args, **kwargs)

            return OVMBartForConditionalGeneration(config, encoder, net, net_past)

        model = MBartForConditionalGeneration.from_pretrained(model_name_or_path, *model_args, **kwargs)
        use_cache = model.config.use_cache

        origin_forward = model.forward

        def forward(*args, **kwargs):
            outputs = origin_forward(*args, **kwargs)

            if outputs.past_key_values is not None:
                # Multiply by 1.0 to workaround a bug in OpenVINO 2022.1 with
                # dynamic shapes inputs connected to model outputs:
                past_key_values = []
                for i in range(12):
                    past_key_values.append(
                        (
                            outputs.past_key_values[i][0],
                            outputs.past_key_values[i][1],
                            outputs.past_key_values[i][2] * 1.0,
                            outputs.past_key_values[i][3] * 1.0,
                        )
                    )
                outputs.past_key_values = tuple(past_key_values)

            return Seq2SeqLMOutput(
                logits=outputs.logits,
                past_key_values=outputs.past_key_values,
            )

        model.forward = lambda *args, **kwargs: forward(*args, **kwargs)

        # Create a separate network for encoder - it will be called just once.
        encoder = load_ov_model_from_pytorch(model.get_encoder())

        inputs = {
            "input_ids": None,
            "attention_mask": torch.zeros([1, 11], dtype=torch.int32),
            "decoder_input_ids": torch.zeros([1, 1 if use_cache else 11], dtype=torch.int32),
            "decoder_attention_mask": None,
            "head_mask": None,
            "decoder_head_mask": None,
            "cross_attn_head_mask": None,
            "encoder_outputs": [torch.zeros([1, 11, 1024], dtype=torch.float32)],
        }

        net = load_ov_model_from_pytorch(model, inputs)

        # Fix for 2022.1 release
        if is_openvino_api_2:
            net.net.inputs[2].get_tensor().set_names(set(["encoder_outputs"]))

        if use_cache:
            inputs["past_key_values"] = [
                [
                    torch.zeros([1, 16, 1, 64], dtype=torch.float32),
                    torch.zeros([1, 16, 1, 64], dtype=torch.float32),
                    torch.zeros([1, 16, 11, 64], dtype=torch.float32),
                    torch.zeros([1, 16, 11, 64], dtype=torch.float32),
                ]
            ] * 12
            net_past = load_ov_model_from_pytorch(model, inputs)
        else:
            net_past = None

        return OVMBartForConditionalGeneration(model.config, encoder, net, net_past)
