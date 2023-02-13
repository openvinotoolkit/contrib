// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "openvino/cc/ngraph/itt.hpp"
#include "cuda_fullyconnected_transformation.hpp"

#include <exec_graph_info.hpp>
#include "openvino/pass/pattern/op/wrap_type.hpp"
#include <ngraph/rt_info.hpp>
#include <openvino/op/add.hpp>
#include <openvino/op/matmul.hpp>
#include <ops/matmul.hpp>
#include <transformer/nodes/fully_connected.hpp>

using namespace ov::pass::pattern;

namespace ov::nvidia_gpu::pass {
bool fuse_matmul_and_add(Matcher &m) {
    // Decompose Divide into Multiply with Power operations
    auto matMulNode = std::dynamic_pointer_cast<ov::op::v0::MatMul>(m.get_match_root());
    const auto matMulNodeOutputInputs = matMulNode->output(0).get_target_inputs();
    if (matMulNodeOutputInputs.empty()) {
        return false;
    }
    const auto addNode = matMulNodeOutputInputs.begin()->get_node()->shared_from_this();
    if (!std::dynamic_pointer_cast<ov::op::v1::Add>(addNode)) {
        return false;
    }

    std::shared_ptr<Node> constantNode;
    if (matMulNode == addNode->get_input_node_shared_ptr(0)) {
        constantNode = addNode->get_input_node_shared_ptr(1);
    } else {
        constantNode = addNode->get_input_node_shared_ptr(0);
    }
    if (!std::dynamic_pointer_cast<ov::op::v0::Constant>(constantNode)) {
        return false;
    }

    auto matrixAShape = matMulNode->get_input_shape(0);
    auto matrixBShape = matMulNode->get_input_shape(1);
    const auto matrixShape = matMulNode->get_output_shape(0);
    ov::nvidia_gpu::MatMulOp::BroadcastToMatrix(matrixAShape);
    ov::nvidia_gpu::MatMulOp::BroadcastToMatrix(matrixBShape);
    const auto matMulBatch = std::max(ov::nvidia_gpu::MatMulOp::GetMatrixNumBatches(matrixAShape),
                                      ov::nvidia_gpu::MatMulOp::GetMatrixNumBatches(matrixBShape));

    auto constShape = constantNode->get_output_shape(0);
    ov::nvidia_gpu::MatMulOp::BroadcastToMatrix(constShape);
    const auto constBatch = ov::nvidia_gpu::MatMulOp::GetMatrixNumBatches(constShape);
    const auto constShapeSize = ov::shape_size(constShape);
    const auto matrixShapeSize = ov::shape_size(matrixShape);
    const auto numAutoConstBatch = matrixShapeSize / constShapeSize;
    const auto matmulShapeDividable = matrixShapeSize % constShapeSize;
    if (matMulBatch < constBatch || matmulShapeDividable != 0 || numAutoConstBatch > 1) {
        return false;
    }

    const auto fullyConnectedNode =
        std::make_shared<ov::nvidia_gpu::nodes::FullyConnected>(matMulNode->get_input_node_shared_ptr(0),
                                                            matMulNode->get_input_node_shared_ptr(1),
                                                            constantNode,
                                                            matMulNode->get_transpose_a(),
                                                            matMulNode->get_transpose_b());
    fullyConnectedNode->set_friendly_name(addNode->get_friendly_name());
    ov::copy_runtime_info({matMulNode, addNode}, fullyConnectedNode);

    const std::string originalLayers = matMulNode->get_friendly_name() + "," + addNode->get_friendly_name();
    fullyConnectedNode->get_rt_info()[ExecGraphInfoSerialization::ORIGINAL_NAMES] = originalLayers;

    ov::replace_node(addNode, fullyConnectedNode);
    ov::replace_node(matMulNode, fullyConnectedNode);

    return true;
}

FullyConnectedTransformation::FullyConnectedTransformation() {
    MATCHER_SCOPE(FullyConnectedTransformation);
    auto matmul = wrap_type<ov::op::v0::MatMul>(consumers_count(1));

    matcher_pass_callback callback = [](Matcher &m) { return fuse_matmul_and_add(m); };

    auto m = std::make_shared<Matcher>(matmul, matcher_name);
    register_matcher(m, callback);
}

}  // namespace ov::nvidia_gpu::pass
