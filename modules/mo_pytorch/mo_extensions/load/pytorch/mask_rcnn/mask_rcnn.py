import torch

from ..hooks import OpenVINOTensor, forward_hook

def rpn_forward(self, images, features, targets=None):
    from torchvision.models.detection.rpn import concat_box_prediction_layers
    # Copy of torchvision/models/detection/rpn.py
    features = list(features.values())
    objectness, pred_bbox_deltas = self.head(features)
    anchors = self.anchor_generator(images, features)

    objectness, pred_bbox_deltas = concat_box_prediction_layers(objectness, pred_bbox_deltas)
    # end of copy

    # [19200, 4800, 1200, 300, 75]
    # Create an alias
    class DetectionOutput(torch.nn.Module):
        def __init__(self, top_k):
            super().__init__()
            self.variance_encoded_in_target = True
            self.nms_threshold = 0.7
            self.confidence_threshold = -999
            self.top_k = 25575
            self.keep_top_k = top_k
            self.code_type = 'caffe.PriorBoxParameter.CENTER_SIZE'
            self.background_label_id = 100

        def infer_shapes(self, inputs):
            return [1, 1, self.keep_top_k, 7]


    # outputs = [OpenVINOTensor(), OpenVINOTensor()]
    # for out in outputs:
    #     out.graph = objectness.graph

    # torch.Size([1, 1, 76824])
    # (1, 76824)
    # (1, 1536480)
    # print(anchors.shape)
    # print(deltas.shape)
    # print(logist.shape)
    # anchors = anchors[0].reshape(1, 1, -1)
    # anchors /= 320
    # pred_bbox_deltas = pred_bbox_deltas.reshape(1, -1)
    # objectness = objectness.reshape(1, -1)
    anchors = anchors[0].reshape(1, -1, 4)
    anchors /= 320
    pred_bbox_deltas = pred_bbox_deltas.reshape(1, -1, 4)
    objectness = objectness.reshape(1, -1).sigmoid()

    start_idx = 0
    all_proposals = []
    for shape in [19200, 4800, 1200, 300, 75]:
        end_idx = start_idx + shape
        scores = objectness[:, start_idx : end_idx]
        deltas = pred_bbox_deltas[:, start_idx : end_idx].reshape(1, -1)
        priors = anchors[:, start_idx : end_idx].reshape(1, 1, -1)
        proposals = forward_hook(DetectionOutput(top_k=min(shape, 1000)), (deltas, scores, OpenVINOTensor(priors)))
        all_proposals.append(proposals)
        start_idx = end_idx

    all_proposals = torch.cat(all_proposals, dim=2)

    _, ids = torch.topk(all_proposals[0, 0, :, 2], 1000)
    all_proposals = torch.gather(all_proposals, 2, ids).reshape(-1, 7)[:, 3:]
    return [all_proposals, OpenVINOTensor()]


    # start_idx = 0
    # all_indices = []
    # for shape in [19200, 4800, 1200, 300, 75]:
    #     end_idx = start_idx + shape
    #     scores = objectness[0, start_idx : end_idx]
    #     topk, indices = torch.topk(scores, min(shape, 1000))
    #     indices += start_idx

    #     all_indices.append(indices)
    #     start_idx = end_idx

    # all_indices_2 = torch.cat(all_indices)

    # pred_bbox_deltas = torch.gather(pred_bbox_deltas.reshape(1, -1, 4), 1, all_indices_2).reshape(1, -1)
    # objectness = torch.gather(objectness, 1, all_indices_2)

    # anchors = OpenVINOTensor(anchors)
    # anchors.graph = objectness.graph
    # anchors = torch.gather(anchors.reshape(1, -1, 4), 1, all_indices_2).reshape(1, 1, -1)

    proposals = forward_hook(DetectionOutput(1000), (pred_bbox_deltas, objectness, OpenVINOTensor(anchors)))
    # proposals = proposals.reshape(-1, 7)[:, 3:]
    proposals = proposals.reshape(-1, 7)
    return [proposals, OpenVINOTensor()]


def multi_scale_roi_align(cls, features, proposals, num_proposals, output_size):
    scales = [0.25, 0.125, 0.0625, 0.03125]
    zeros = OpenVINOTensor(torch.zeros([num_proposals], dtype=torch.int32))

    # Proposals are in absolute coordinates
    proposals = proposals * 320
    levels = cls.box_roi_pool.map_levels([proposals]).reshape(-1, 1, 1, 1)

    final_box_features = None
    for i in range(4):
        class ROIAlign(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.pooled_h = output_size
                self.pooled_w = output_size
                self.sampling_ratio = 2
                self.mode = 'avg'
                self.spatial_scale = scales[i]


        box_features = forward_hook(ROIAlign(), (features[str(i)], proposals, zeros))

        # Imitation of level-wise ROIAlign
        box_features = box_features * (levels == i).to(torch.float32)
        if i > 0:
            final_box_features += box_features
        else:
            final_box_features = box_features

    return final_box_features


def roi_heads_forward(self, features, proposals, image_shapes, targets=None):
    box_features = multi_scale_roi_align(self, features, proposals, 1000, output_size=7)
    box_features = self.box_head(box_features)
    class_logits, box_regression = self.box_predictor(box_features)

    class DetectionOutput(torch.nn.Module):
        def __init__(self):
            super().__init__()
            self.variance_encoded_in_target = True
            self.nms_threshold = 0.5
            self.confidence_threshold = 0.05
            self.top_k = 6000
            self.keep_top_k = 100
            self.code_type = 'caffe.PriorBoxParameter.CENTER_SIZE'
            self.background_label_id = 0

        def infer_shapes(self, inputs):
            return [1, 1, self.keep_top_k, 7]

    proposal = proposals.reshape(1, 1, -1)
    box_regression = box_regression.reshape(1, -1, 4)
    box_regression /= torch.tensor([10.0, 10.0, 5.0, 5.0])
    box_regression = box_regression.reshape(1, -1)
    class_logits = class_logits.softmax(1).reshape(1, -1)

    detections = forward_hook(DetectionOutput(), (box_regression, class_logits, proposal))

    # Predict masks
    # boxes = detections[0, 0, :, 3:]
    # mask_features = multi_scale_roi_align(self, features, boxes, 100, output_size=14)
    # mask_features = self.mask_head(mask_features)
    # mask_logits = self.mask_predictor(mask_features)

    return {'boxes': detections, 'masks': None}, None



class MaskRCNN(object):
    def __init__(self):
        self.class_name = 'torchvision.models.detection.mask_rcnn.MaskRCNN'


    def register_hook(self, model):
        model.rpn.forward = lambda *args: rpn_forward(model.rpn, *args)
        model.roi_heads.forward = lambda *args: roi_heads_forward(model.roi_heads, *args)
        model.transform.postprocess = lambda *args: args[0]
