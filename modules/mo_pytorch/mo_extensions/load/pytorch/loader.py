"""
 Copyright (C) 2020-2023 Intel Corporation

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
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import logging as log

import numpy as np

import torch
from torch.autograd import Variable

from openvino.tools.mo.load.loader import Loader
from openvino.tools.mo.middle.ConvertGroupedStridedSlice import ConvertGroupedStridedSlice
# TODO: fix this bug
ConvertGroupedStridedSlice.enabled = False
from openvino.tools.mo.front.common.register_custom_ops import update_extractors_with_extensions, check_for_duplicates
from openvino.tools.mo.front.extractor import extract_node_attrs
from openvino.tools.mo.graph.graph import Node, Graph

from .hooks import OpenVINOTensor, forward_hook
from .model_hooks import register_model_hook


pytorch_op_extractors = {}
def pytorch_op_extractor(node: Node, lowered_keys_map: dict) -> (bool, dict):
    result = {}
    supported = False
    op = node['op'].lower()
    if op in lowered_keys_map:
        op = lowered_keys_map[op]
        if not op in pytorch_op_extractors:
            raise Error('Op ' + op + ' is not registered')
        attrs = pytorch_op_extractors[op](node)
        if attrs:
            result.update(attrs)
            supported = True
    return supported, result


class PyTorchLoader(Loader):
    enabled = True

    def load(self, graph: Graph):
        argv = graph.graph['cmd_params']
        graph.graph['fw'] = 'pytorch'
        graph.graph['layout'] = 'NCHW'

        update_extractors_with_extensions(pytorch_op_extractors)

        # Create a dummy input
        if argv.input:
            placeholder_shapes = argv.placeholder_shapes
            placeholder_data_types = argv.placeholder_data_types
        else:
            placeholder_shapes = {'input': argv.placeholder_shapes}
            placeholder_data_types = {'input': np.float32}

        inputs = {}
        for name, shape in placeholder_shapes.items():
            dtype = placeholder_data_types[name]
            inp = np.random.randint(0, 255, shape).astype(dtype)
            inp = OpenVINOTensor(torch.tensor(inp))

            inputs[name] = inp
            inp.graph = graph
            inp.node_name = name
            graph.add_node(name, kind='op', op='Parameter', name=name, shape=shape)

        for name, value in argv.freeze_placeholder_with_value.items():
            inputs[name] = value

        model = argv.input_model

        register_model_hook(model, argv.is_dynamic)

        with torch.no_grad():
            if argv.input:
                model(**inputs)
            else:
                model(inputs['input'])

        extract_node_attrs(graph, lambda node: pytorch_op_extractor(node, check_for_duplicates(pytorch_op_extractors)))
