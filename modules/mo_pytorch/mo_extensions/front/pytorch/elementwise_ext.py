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

from extensions.ops.elementwise import *
from mo.front.extractor import FrontExtractorOp
from mo.graph.graph import Node
from mo.ops.eltwise_n import EltwiseNAdd, EltwiseNMax
from mo.ops.power import AttributedPower


class AddFrontExtractor(FrontExtractorOp):
    op = 'Add'
    enabled = True

    @classmethod
    def extract(cls, node: Node):
        Add.update_node_stat(node)
        return cls.enabled


class SubFrontExtractor(FrontExtractorOp):
    op = 'Sub'
    enabled = True

    @classmethod
    def extract(cls, node: Node):
        Sub.update_node_stat(node)
        return cls.enabled


class MulFrontExtractor(FrontExtractorOp):
    op = 'Mul'
    enabled = True

    @classmethod
    def extract(cls, node: Node):
        Mul.update_node_stat(node)
        return cls.enabled
