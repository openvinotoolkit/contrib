"""
 Copyright (C) 2018-2023 Intel Corporation

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

from openvino.tools.mo.ops.pack import PackOp
from openvino.tools.mo.front.extractor import FrontExtractorOp


class StackFrontExtractor(FrontExtractorOp):
    op = 'Stack'
    enabled = True

    @classmethod
    def extract(cls, node):
        update_attrs = {
            'axis': node.module.dim,
        }

        # update the attributes of the node
        PackOp.update_node_stat(node, update_attrs)

        return cls.enabled
