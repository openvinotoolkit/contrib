"""
 Copyright (C) 2020 Intel Corporation

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

import torch
from torch.autograd import Variable

from extensions.load.loader import Loader
from extensions.middle.ConvertGroupedStridedSlice import ConvertGroupedStridedSlice
# TODO: fix this bug
ConvertGroupedStridedSlice.enabled = False
from mo.front.common.register_custom_ops import update_extractors_with_extensions, check_for_duplicates
from mo.front.extractor import extract_node_attrs
from mo.graph.graph import Node, Graph

from .hooks import OpenVINOTensor, forward_hook
from .model_hooks import register_model_hook


pytorch_op_extractors = {}
def pytorch_op_extractor(node: Node, lowered_keys_map: dict) -> (bool, dict):
    result = {}
    supported = False
    op = node['op'].lower()
    if op in lowered_keys_map:
        op = lowered_keys_map[op]
        assert op in pytorch_op_extractors
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
        inp = OpenVINOTensor(torch.randn(list(argv.placeholder_shapes)))
        inp.graph = graph
        inp.node_name = 'input'

        model = argv.input_model

        for module in model.modules():
            if len([m for m in module.modules()]) != 1:
                continue
            module.register_forward_hook(forward_hook)

        register_model_hook(model)

        graph.add_node('input', kind='op', op='Parameter', name='input', shape=list(inp.shape))

        with torch.no_grad():
            outs = model(inp)

        # Add output nodes
        if not hasattr(outs, '__contains__'):  # if a single tensor
            outs = [outs]
        if isinstance(outs, dict):
            outs = outs.values()

        for out in outs:
            name = out.node_name
            graph.add_node('output', kind='op', op='Result')
            edge_attrs = {
                'out': 0,
                'in': 0,
                'name': name,
                'fw_tensor_debug_info': [(name, name)],
                'in_attrs': ['in', 'name'],
                'out_attrs': ['out', 'name'],
                'data_attrs': ['fw_tensor_debug_info']
            }
            graph.add_edge(name, 'output', **edge_attrs)

        extract_node_attrs(graph, lambda node: pytorch_op_extractor(node, check_for_duplicates(pytorch_op_extractors)))
