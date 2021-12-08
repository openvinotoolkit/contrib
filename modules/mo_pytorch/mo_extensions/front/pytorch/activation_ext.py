"""
 Copyright (C) 2018-2020 Intel Corporation

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
"""
from openvino.tools.mo.ops.activation_ops import *
from openvino.tools.mo.front.extractor import FrontExtractorOp

class ReLUExtractor(FrontExtractorOp):
    op = 'ReLU'
    enabled = True

    @classmethod
    def extract(cls, node):
        ReLU.update_node_stat(node)
        return cls.enabled


class LeakyReLUExtractor(FrontExtractorOp):
    op = 'LeakyReLU'
    enabled = True

    @classmethod
    def extract(cls, node):
        attrs = {
            'negative_slope': node.module.negative_slope,
        }
        LeakyReLU.update_node_stat(node, attrs)
        return cls.enabled


class SigmoidExtractor(FrontExtractorOp):
    op = 'Sigmoid'
    enabled = True

    @classmethod
    def extract(cls, node):
        Sigmoid.update_node_stat(node)
        return cls.enabled


class TanhExtractor(FrontExtractorOp):
    op = 'Tanh'
    enabled = True

    @classmethod
    def extract(cls, node):
        Tanh.update_node_stat(node)
        return cls.enabled
