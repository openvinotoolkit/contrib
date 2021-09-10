import os
import sys
import unittest
import numpy as np
import torch
from pathlib import Path
import torchvision.models as models
from detectron2 import model_zoo

import cv2 as cv

from openvino.inference_engine import IECore
import mo_pytorch

class TestModels(unittest.TestCase):

    @classmethod
    def setUpClass(self):
        self.ie = IECore()
        self.test_img = cv.imread(os.path.join(os.environ['MODELS_PATH'],
                                               'validation_set',
                                               '512x512',
                                               'dog.bmp'))
        if self.test_img is None:
            tc = unittest.TestCase()
            tc.fail('No image data found')


    def get_iou(self, box1, box2):
        # box is xmin, ymin, xmax, ymax
        x_min, x_max = max(box1[0], box2[0]), min(box1[2], box2[2])
        y_min, y_max = max(box1[1], box2[1]), min(box1[3], box2[3])
        inter = (x_max - x_min) * (y_max - y_min)
        area1 = (box1[2] - box1[0]) * (box1[3] - box1[1])
        area2 = (box2[2] - box2[0]) * (box2[3] - box2[1])
        return inter / (area1 + area2 - inter)


    # source: https://github.com/opencv/opencv/blob/master/modules/dnn/misc/python/test/test_dnn.py
    def normAssertDetections(self, ref_class_ids, ref_scores, ref_boxes,
                             test_class_ids, test_scores, test_boxes,
                             conf_threshold=1e-5, scores_diff=1e-5, boxes_iou_diff=1e-4):
        matched_ref_boxes = [False] * len(ref_boxes)
        errMsg = ''
        for i in range(len(test_boxes)):
            test_score = test_scores[i]
            if test_score < conf_threshold:
                continue

            test_class_id, test_box = test_class_ids[i], test_boxes[i]
            matched = False
            for j in range(len(ref_boxes)):
                if (not matched_ref_boxes[j]) and test_class_id == ref_class_ids[j] and \
                   abs(test_score - ref_scores[j]) < scores_diff:
                    iou = self.get_iou(test_box, ref_boxes[j])
                    if abs(iou - 1.0) < boxes_iou_diff:
                        matched = True
                        matched_ref_boxes[j] = True
            if not matched:
                errMsg += '\nUnmatched prediction: class %d score %f box %s' % (test_class_id, test_score, test_box)

        for i in range(len(ref_boxes)):
            if (not matched_ref_boxes[i]) and ref_scores[i] > conf_threshold:
                errMsg += '\nUnmatched reference: class %d score %f box %s' % (ref_class_ids[i], ref_scores[i], ref_boxes[i])
        if errMsg:
            raise Exception(errMsg)


    def check_torchvision_model(self, model_func, size, threshold=1e-5):
        inp_size = [1, 3, size[0], size[1]]

        inp = cv.resize(self.test_img, (size[1], size[0]))
        inp = np.expand_dims(inp.astype(np.float32).transpose(2, 0, 1), axis=0)
        inp /= 255
        inp = torch.tensor(inp)

        # Create model
        model = model_func(pretrained=True, progress=False)
        model.eval()
        ref = model(inp)

        # Forward random input through the model to check that nothing got stuck from reference dat
        rand_inp = torch.rand(inp.size(), dtype=inp.dtype)
        model(rand_inp)

        # Convert to OpenVINO IR
        mo_pytorch.convert(model, input_shape=inp_size, model_name='model')

        # Run model with OpenVINO and compare outputs
        net = self.ie.read_network('model.xml', 'model.bin')
        exec_net = self.ie.load_network(net, 'CPU')
        out = exec_net.infer({'input': inp.detach().numpy()})

        if isinstance(ref, torch.Tensor):
            ref = {'': ref}
        for out0, ref0 in zip(out.values(), ref.values()):
            diff = np.max(np.abs(out0 - ref0.detach().numpy()))
            self.assertLessEqual(diff, threshold)

    def test_inception_v3(self):
        self.check_torchvision_model(models.inception_v3, (299, 299), 4e-5)

    def test_squeezenet1_1(self):
        self.check_torchvision_model(models.squeezenet1_1, (227, 227))

    def test_alexnet(self):
        self.check_torchvision_model(models.alexnet, (227, 227))

    def test_resnet18(self):
        self.check_torchvision_model(models.resnet18, (227, 227), 2e-5)

    def test_deeplabv3_resnet50(self):
        self.check_torchvision_model(models.segmentation.deeplabv3_resnet50, (240, 320), 2e-4)

    def test_detectron2_retinanet(self):
        width = 320
        height = 320

        # Load model
        model = model_zoo.get("COCO-Detection/retinanet_R_50_FPN_1x.yaml", trained=True)
        model.eval()

        # Prepare input tensor
        img = cv.resize(self.test_img, (width, height))
        inp = img.transpose(2, 0, 1).astype(np.float32)

        # Get reference prediction
        ref = model([{'image': torch.tensor(inp)}])
        ref = ref[0]['instances'].get_fields()
        ref_boxes = []
        for box, score, class_idx in zip(ref['pred_boxes'], ref['scores'], ref['pred_classes']):
            xmin, ymin, xmax, ymax = box
            ref_boxes.append([xmin, ymin, xmax, ymax])
            if score > 0.45:
                cv.rectangle(img, (int(xmin), int(ymin)), (int(xmax), int(ymax)), color=(0, 180, 255), thickness=3)

        # Convert model to OpenVINO IR
        mo_pytorch.convert(model, input_shape=[1, 3, height, width], model_name='model')

        # Get OpenVINO prediction
        net = self.ie.read_network('model.xml')
        exec_net = self.ie.load_network(net, 'CPU')
        outs = exec_net.infer({'input': inp.reshape(1, 3, height, width)})
        ie_detections = next(iter(outs.values()))
        ie_detections = ie_detections.reshape(-1, 7)

        for det in ie_detections:
            conf = det[2]
            if conf > 0.45:
                xmin, ymin, xmax, ymax = [int(v) for v in det[3:]]
                cv.rectangle(img, (xmin, ymin), (xmax, ymax), color=(210, 9, 179))

        # Uncomment to visualize detections
        # cv.imshow('RetinaNet (Detectron2)', img)
        # cv.waitKey()

        self.normAssertDetections(ref['pred_classes'], ref['scores'], ref_boxes,
                                  ie_detections[:, 1], ie_detections[:, 2], ie_detections[:, 3:])

    def test_strided_slice(self):
        import torch.nn as nn
        class SSlice(nn.Module):
            def forward(self, x):
                return x[:, :1, 2:, 3]

        self.check_torchvision_model(lambda **args: SSlice(), (299, 299), 4e-5)


    def test_resunet(self):
        import BrainMaGe
        from BrainMaGe.models.networks import fetch_model

        weights = Path(BrainMaGe.__file__).parent / 'weights' / 'resunet_ma.pt'
        pt_model = fetch_model(modelname="resunet", num_channels=1, num_classes=2, num_filters=16)
        checkpoint = torch.load(weights, map_location=torch.device('cpu'))
        pt_model.load_state_dict(checkpoint["model_state_dict"])
        pt_model.eval()

        # Get reference output
        inp = torch.randn([1, 1, 128, 128, 128])
        ref = pt_model(inp).detach().numpy()

        # Perform multiple runs with other inputs to make sure that InstanceNorm layer does not stuck
        for _ in range(2):
            dummy_inp = torch.randn(inp.shape)
            pt_model(dummy_inp)

        # Generate OpenVINO IR
        mo_pytorch.convert(pt_model, input_shape=list(inp.shape), model_name='model')

        # Run model with OpenVINO and compare outputs
        net = self.ie.read_network('model.xml', 'model.bin')
        exec_net = self.ie.load_network(net, 'CPU')
        out = exec_net.infer({'input': inp.detach().numpy()})
        out = next(iter(out.values()))

        diff = np.max(np.abs(out - ref))
        self.assertLessEqual(diff, 5e-4)

    def test_rugpt3(self):
        from transformers import GPT2LMHeadModel, GPT2Tokenizer

        model_name_or_path = "sberbank-ai/rugpt3medium_based_on_gpt2"
        tokenizer = GPT2Tokenizer.from_pretrained(model_name_or_path)
        model = GPT2LMHeadModel.from_pretrained(model_name_or_path)

        text = "Александр Сергеевич Пушкин родился в "
        input_ids = tokenizer.encode(text, return_tensors="pt")
        seq_len = input_ids.shape[1]
        result = model(input_ids)

        # Forward random input through the model to check that nothing got stuck from reference dat
        dummy_inp = torch.randint(0, 255, input_ids.shape)
        model(dummy_inp)

        # Generate OpenVINO IR
        mo_pytorch.convert(model, input_shape='[1, {}],[{}]'.format(seq_len, seq_len),
                           input='input_ids{i64},position_ids{i64}', model_name='model')

        # Run model with OpenVINO and compare outputs
        net = self.ie.read_network('model.xml', 'model.bin')
        exec_net = self.ie.load_network(net, 'CPU')
        out = exec_net.infer({'input_ids': input_ids, 'position_ids': np.arange(seq_len)})
        out = next(iter(out.values()))

        ref = result[0].detach().numpy()
        diff = np.max(np.abs(out - ref))
        self.assertLessEqual(diff, 1e-4)


    def test_usrnet(self):
        sys.path.append('USRNet')
        from models.network_usrnet import USRNet as net   # for pytorch version <= 1.7.1
        from utils import utils_deblur
        from utils import utils_sisr as sr
        from utils import utils_image as util

        np.random.seed(324)
        torch.manual_seed(32)
        inp = np.random.standard_normal([1, 3, 56, 112]).astype(np.float32)
        sf = 4

        # source: https://github.com/cszn/USRNet/blob/master/main_test_realapplication.py
        def get_kernel_sigma():
            noise_level_img = 2       # noise level for LR image, 0.5~3 for clean images
            kernel_width_default_x1234 = [0.4, 0.7, 1.5, 2.0] # default Gaussian kernel widths of clean/sharp images for x1, x2, x3, x4

            noise_level_model = noise_level_img/255.  # noise level of model
            kernel_width = kernel_width_default_x1234[sf-1]

            k = utils_deblur.fspecial('gaussian', 25, kernel_width)
            k = sr.shift_pixel(k, sf)  # shift the kernel
            k /= np.sum(k)
            kernel = util.single2tensor4(k[..., np.newaxis])
            sigma = torch.tensor(noise_level_model).float().view([1, 1, 1, 1])

            return kernel, sigma

        kernel, sigma = get_kernel_sigma()

        model = net(n_iter=8, h_nc=64, in_nc=4, out_nc=3, nc=[64, 128, 256, 512],
                    nb=2, act_mode="R", downsample_mode='strideconv', upsample_mode="convtranspose")
        model.eval()

        ref = model(torch.tensor(inp), kernel, sf, sigma)

        # Forward random input through the model to check that nothing got stuck from reference data
        dummy_inp = torch.randn(inp.shape)
        dummy_kernel = torch.randn(kernel.shape)
        dummy_sigma = torch.randn(sigma.shape)
        model(dummy_inp, dummy_kernel, sf, dummy_sigma)

        # Generate OpenVINO IR
        mo_pytorch.convert(model, input_shape='[1, 3, 56, 112],[1, 1, 25, 25],[1, 1, 1, 1],[1]',
                           input='x{f32},k{f32},sigma{f32},sf{f32}->4',
                           model_name='model')

        # Run model with OpenVINO and compare outputs
        net = self.ie.read_network('model.xml')
        exec_net = self.ie.load_network(net, 'CPU')
        out = exec_net.infer({'x': inp, 'k': kernel, 'sigma': sigma})
        out = next(iter(out.values()))

        diff = np.max(np.abs(ref.detach().numpy() - out))
        self.assertLessEqual(diff, 1e-4)


if __name__ == '__main__':
    unittest.main()
