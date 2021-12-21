import numpy as np
import torch

from ..hooks import OpenVINOTensor, forward_hook

class DetectionOutput(torch.nn.Module):
    def __init__(self, top_k, nms_threshold, confidence_threshold, background_label_id):
        super().__init__()
        self.variance_encoded_in_target = True
        self.nms_threshold = nms_threshold
        self.confidence_threshold = confidence_threshold
        self.top_k = top_k
        self.keep_top_k = top_k
        self.code_type = 'caffe.PriorBoxParameter.CENTER_SIZE'
        self.background_label_id = background_label_id
        self.clip_before_nms = True
        self.share_location = False

    def infer_shapes(self, inputs):
        return [1, 1, self.keep_top_k, 7]


def rpn_forward(self, images, features, targets=None):
    from torchvision.models.detection.rpn import concat_box_prediction_layers
    # Copy of torchvision/models/detection/rpn.py
    features = list(features.values())
    objectness, pred_bbox_deltas = self.head(features)
    anchors = self.anchor_generator(images, features)

    shapes = [np.prod(o.shape) for o in objectness]

    objectness, pred_bbox_deltas = concat_box_prediction_layers(objectness, pred_bbox_deltas)
    # end of copy

    img_h, img_w = images.tensors.shape[2:]
    anchors = anchors[0].reshape(1, -1, 4)
    anchors /= torch.tensor([img_w, img_h, img_w, img_h])
    pred_bbox_deltas = pred_bbox_deltas.reshape(1, -1, 4)
    objectness = objectness.reshape(1, -1).sigmoid()

    start_idx = 0
    all_proposals = []
    for shape in shapes:
        end_idx = start_idx + shape
        scores = objectness[:, start_idx : end_idx]
        deltas = pred_bbox_deltas[:, start_idx : end_idx].reshape(1, -1)
        priors = anchors[:, start_idx : end_idx].reshape(1, 1, -1)

        det = DetectionOutput(top_k=min(shape, self.post_nms_top_n()),
                              nms_threshold=self.nms_thresh,
                              confidence_threshold=0.0,
                              background_label_id=2)
        proposals = forward_hook(det, (deltas, scores, OpenVINOTensor(priors)))
        all_proposals.append(proposals)
        start_idx = end_idx

    all_proposals = torch.cat(all_proposals, dim=2)

    _, ids = torch.topk(all_proposals[0, 0, :, 2], self.post_nms_top_n())
    all_proposals = torch.gather(all_proposals, 2, ids).reshape(-1, 7)[:, 3:]
    return [all_proposals, OpenVINOTensor()]


def multi_scale_roi_align(cls, features, proposals, image_shapes):
    num_proposals = proposals.shape[0]
    zeros = OpenVINOTensor(torch.zeros([num_proposals], dtype=torch.int32))

    # Proposals are in absolute coordinates
    img_h, img_w = image_shapes[0]
    proposals = proposals * torch.tensor([img_w, img_h, img_w, img_h])

    if cls.scales is None:
        x_filtered = [features[k] for k in cls.featmap_names]
        cls.setup_scales(x_filtered, image_shapes)

    levels = cls.map_levels([proposals]).reshape(-1, 1, 1, 1)

    final_box_features = None
    for lvl, name in enumerate(cls.featmap_names):
        class ROIAlign(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.pooled_h = cls.output_size[0]
                self.pooled_w = cls.output_size[1]
                self.sampling_ratio = 2
                self.mode = 'avg'
                self.spatial_scale = cls.scales[lvl]

            def infer_shapes(self, inputs):
                feat_shape = inputs[0].dynamic_shape
                rois_shape = inputs[1].dynamic_shape
                return [rois_shape[0], feat_shape[1], self.pooled_h, self.pooled_w]


        box_features = forward_hook(ROIAlign(), (features[name], proposals, zeros))

        # Imitation of level-wise ROIAlign
        box_features = box_features * (levels == lvl).to(torch.float32)
        if lvl > 0:
            final_box_features += box_features
        else:
            final_box_features = box_features

    return final_box_features


def roi_heads_forward(self, features, proposals, image_shapes, targets=None):
    box_features = multi_scale_roi_align(self.box_roi_pool, features, proposals, image_shapes)
    box_features = self.box_head(box_features)
    class_logits, box_regression = self.box_predictor(box_features)

    proposal = proposals.reshape(1, 1, -1)
    box_regression = box_regression.reshape(1, -1, 4)
    box_regression /= self.box_coder.weights
    box_regression = box_regression.reshape(1, -1)
    class_logits = class_logits.softmax(1).reshape(1, -1)

    det = DetectionOutput(top_k=self.detections_per_img,
                          nms_threshold=self.nms_thresh,
                          confidence_threshold=self.score_thresh,
                          background_label_id=0)
    detections = forward_hook(det, (box_regression, class_logits, proposal))

    # Predict masks
    boxes = detections[0, 0, :, 3:]
    mask_features = multi_scale_roi_align(self.mask_roi_pool, features, boxes, image_shapes)
    mask_features = self.mask_head(mask_features)
    mask_logits = self.mask_predictor(mask_features)
    mask_probs = mask_logits.sigmoid()

    return {'boxes': detections.clone(), 'masks': mask_probs}, {}


def model_forward(self, images, targets=None):
    from torchvision.models.detection.image_list import ImageList
    images = self.transform.normalize(images)
    img_h, img_w = images.shape[2:]
    original_image_sizes = [(img_h, img_w)]
    images = ImageList(images, [[img_h, img_w]])

    features = self.backbone(images.tensors)
    proposals, proposal_losses = self.rpn(images, features, targets)
    detections, detector_losses = self.roi_heads(features, proposals, images.image_sizes, targets)
    return detections


class MaskRCNN(object):
    def __init__(self):
        self.class_name = 'torchvision.models.detection.mask_rcnn.MaskRCNN'


    def register_hook(self, model):
        model.forward = lambda *args: model_forward(model, *args)
        model.rpn.forward = lambda *args: rpn_forward(model.rpn, *args)
        model.roi_heads.forward = lambda *args: roi_heads_forward(model.roi_heads, *args)
