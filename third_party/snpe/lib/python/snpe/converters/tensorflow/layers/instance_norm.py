#!/usr/bin/env python
# =============================================================================
#
#  Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
#  All Rights Reserved.
#  Confidential and Proprietary - Qualcomm Technologies, Inc.
#
# =============================================================================

import numpy as np

from snpe.converters.common.converter_ir.op_adapter import BatchnormOp
from snpe.converters.tensorflow.common import LayerDescriptor, LayerResolver, LayerBuilder
from snpe.converters.tensorflow.graph_matcher import (
    ConverterSequenceNode,
    GraphSequence,
    NonConsumableConverterSequenceNode
)

class InstanceNormLayerResolver(LayerResolver, object):
    class Descriptor(LayerDescriptor):
        def __init__(self, name, operations, shape):
            super(InstanceNormLayerResolver.Descriptor, self).__init__('InstanceNorm', name, operations)
            self.shape = shape
            # SNPE runtime algo is y = x * WEIGHT / rms + BIAS
            # While L2 Normalization is y = x / rms
            # That requires WEIGHT = 1.0 and BIAS = 0.0 to mimic L2 Norm in SNPE
            # Shape of weights/biases should be same as the last dimension of input.
            self.weights = np.ones(shape[-1])
            self.biases = np.zeros(shape[-1])

    def __init__(self):
        self.sequence = GraphSequence([
            NonConsumableConverterSequenceNode('input', ['?']),
            ConverterSequenceNode('StopGradient', ['StopGradient']),
            ConverterSequenceNode('SquaredDifference', ['SquaredDifference']),
            ConverterSequenceNode('variance', ['Mean']),
            ConverterSequenceNode('add', ['Add']),
            ConverterSequenceNode('mean', ['Mean']),
            NonConsumableConverterSequenceNode('gamma', ['Identity']),
            ConverterSequenceNode('Rsqrt', ['Rsqrt']),
            ConverterSequenceNode('mul_2', ['Mul']),
            NonConsumableConverterSequenceNode('beta', ['Identity']),
            ConverterSequenceNode('mul', ['Mul']),
            ConverterSequenceNode('sub', ['Sub']),
            ConverterSequenceNode('mul_1', ['Mul']),
            ConverterSequenceNode('add_1', ['Add']),
            NonConsumableConverterSequenceNode('stub_14', ['?']),
            NonConsumableConverterSequenceNode('stub_15', ['?']),
            NonConsumableConverterSequenceNode('stub_16', ['?']),
            NonConsumableConverterSequenceNode('stub_17', ['?']),
            NonConsumableConverterSequenceNode('stub_18', ['?']),
        ])
        self.sequence.set_inputs('variance', ['SquaredDifference','stub_14'])
        self.sequence.set_inputs('StopGradient', ['mean'])
        self.sequence.set_inputs('add', ['variance','stub_15'])
        self.sequence.set_inputs('sub', ['beta','mul_2'])
        self.sequence.set_inputs('mean', ['input','stub_16'])
        self.sequence.set_inputs('gamma', ['stub_17'])
        self.sequence.set_inputs('mul_2', ['mean','mul'])
        self.sequence.set_inputs('Rsqrt', ['add'])
        self.sequence.set_inputs('beta', ['stub_18'])
        self.sequence.set_inputs('mul', ['Rsqrt','gamma'])
        self.sequence.set_inputs('add_1', ['mul_1','sub'])
        self.sequence.set_inputs('mul_1', ['input','mul'])
        self.sequence.set_inputs('SquaredDifference', ['input','StopGradient'])
        self.sequence.set_outputs(['add_1'])

    def is_final_resolution(self):
        return True

    def resolve_layer(self, graph_matcher, graph_helper):
        matches = graph_matcher.match_sequence(self.sequence)
        potential_descriptors = []
        for match in matches:
            bn_op = match['SquaredDifference']
            input_op = match['input']

            shape = graph_helper.get_op_output_shape(input_op)

            consumed_nodes = match.consumed_nodes
            potential_descriptors.append(InstanceNormLayerResolver.Descriptor(str(bn_op.name),
                                                           consumed_nodes,
                                                           shape=shape))
        return potential_descriptors

class InstanceNormLayerBuilder(LayerBuilder):
    def build_layer(self, ir_graph, converter_context, descriptor, input_descriptors, output_descriptors):
        """
        :type ir_graph: converters.common.converter_ir.op_graph.IROpGraph
        :type input_descriptors: [converters.tensorflow.common.LayerDescriptor]
        :type output_descriptors: [converters.tensorflow.common.LayerDescriptor]
        :type converter_context: converters.tensorflow.converter.ConverterContext
        :type descriptor: InstanceNormLayerResolver.Descriptor
        :rtype: int
        """
        input_name = self.get_input_name(converter_context, descriptor, input_descriptors)

        return ir_graph.add(BatchnormOp(descriptor.layer_name,
                                        descriptor.weights,
                                        descriptor.biases,
                                        compute_statistics=True,
                                        use_mu_sigma=True,
                                        across_spatial=True),
                            input_names=input_name,
                            output_names=descriptor.output_names[0])
