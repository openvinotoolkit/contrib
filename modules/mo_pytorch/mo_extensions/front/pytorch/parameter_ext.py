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
import numpy as np
from openvino.tools.mo.ops.parameter import Parameter
from openvino.tools.mo.front.extractor import FrontExtractorOp


class PlaceholderFrontExtractor(FrontExtractorOp):
    op = 'Parameter'
    enabled = True

    @classmethod
    def extract(cls, node):
        attrs = {
            'shape': node.shape,
            'data_type': np.float32,  # TODO: other types?
        }
        Parameter.update_node_stat(node, {})
        return cls.enabled
