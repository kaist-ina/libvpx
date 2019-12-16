#!/usr/bin/env python
# =============================================================================
#
#  Copyright (c) 2015-2019 Qualcomm Technologies, Inc.
#  All Rights Reserved.
#  Confidential and Proprietary - Qualcomm Technologies, Inc.
#
# =============================================================================

from snpe.converters.common.converter_ir.op_adapter import ReshapeOp
from snpe.converters.tensorflow.common import LayerDescriptor, LayerResolver, LayerBuilder
from snpe.converters.tensorflow.layers.fullyconnected import FullyConnectedLayerResolver
from snpe.converters.tensorflow.layers.constant import ConstantLayerResolver
from snpe.converters.tensorflow.graph_matcher import (
    ConverterSequenceNode,
    GraphSequence,
    NonConsumableConverterSequenceNode
)
from functools import reduce
import numpy as np

class ReshapeLayerResolver(LayerResolver, object):
    class Descriptor(LayerDescriptor):
        def __init__(self, name, nodes, reshape_op):
            super(ReshapeLayerResolver.Descriptor, self).__init__('Reshape', name, nodes)
            self.reshape_op = reshape_op

    def __init__(self):
        sequence_reshape = GraphSequence([ConverterSequenceNode('root', ['Reshape', 'Squeeze', 'ExpandDims'])])
        sequence_reshape.set_outputs(['root'])

        sequence_flatten = GraphSequence([
            NonConsumableConverterSequenceNode('input', ['?']),
            ConverterSequenceNode('shape', ['Shape']),
            ConverterSequenceNode('slice_1', ['Slice']),
            ConverterSequenceNode('const_1', ['Const']),
            ConverterSequenceNode('const_2', ['Const']),
            ConverterSequenceNode('slice_2', ['Slice']),
            ConverterSequenceNode('const_3', ['Const']),
            ConverterSequenceNode('const_4', ['Const']),
            ConverterSequenceNode('prod', ['Prod']),
            ConverterSequenceNode('const_5', ['Const']),
            ConverterSequenceNode('expand_dims', ['ExpandDims']),
            ConverterSequenceNode('const_6', ['Const']),
            ConverterSequenceNode('concat', ['ConcatV2']),
            ConverterSequenceNode('const_7', ['Const']),
            ConverterSequenceNode('root', ['Reshape']),
        ])
        sequence_flatten.set_inputs('shape', ['input'])
        sequence_flatten.set_inputs('slice_1', ['shape', 'const_1', 'const_2'])
        sequence_flatten.set_inputs('slice_2', ['shape', 'const_3', 'const_4'])
        sequence_flatten.set_inputs('prod', ['slice_2', 'const_5'])
        sequence_flatten.set_inputs('expand_dims', ['prod', 'const_6'])
        sequence_flatten.set_inputs('concat', ['slice_1', 'expand_dims', 'const_7'])
        sequence_flatten.set_inputs('root', ['input', 'concat'])
        sequence_flatten.set_outputs(['root'])

        # consume shape op
        # pattern: shape -> concat -> reshape
        sequence_shape_concat = GraphSequence([
            NonConsumableConverterSequenceNode('input', ['?']),
            ConverterSequenceNode('shape', ['Shape']),
            ConverterSequenceNode('const_1', ['Const']),
            ConverterSequenceNode('const_2', ['Const']),
            ConverterSequenceNode('concat', ['ConcatV2']),
            ConverterSequenceNode('root', ['Reshape']),
        ])
        sequence_shape_concat.set_inputs('shape', ['input'])
        sequence_shape_concat.set_inputs('concat', ['shape', 'const_1', 'const_2'])
        sequence_shape_concat.set_inputs('root', ['concat', 'input'])
        sequence_shape_concat.set_outputs(['root'])

        # consume shape op
        # pattern: shape -> strideslice -> stack -> reshape
        sequence_shape_stridedslice_stack = GraphSequence([
            NonConsumableConverterSequenceNode('input', ['?']),
            ConverterSequenceNode('shape', ['Shape']),
            ConverterSequenceNode('stridedslice', ['StridedSlice']),
            ConverterSequenceNode('const_1', ['Const']),
            ConverterSequenceNode('const_2', ['Const']),
            ConverterSequenceNode('const_3', ['Const']),
            ConverterSequenceNode('const_4', ['Const']),
            ConverterSequenceNode('const_5', ['Const']),
            ConverterSequenceNode('stack', ['Pack']),
            ConverterSequenceNode('root', ['Reshape']),
        ])
        sequence_shape_stridedslice_stack.set_inputs('shape', ['input'])
        sequence_shape_stridedslice_stack.set_inputs('stridedslice', ['shape', 'const_1', 'const_2', 'const_3'])
        sequence_shape_stridedslice_stack.set_inputs('stack', ['stridedslice', 'const_4', 'const_5'])
        sequence_shape_stridedslice_stack.set_inputs('root', ['input', 'stack'])
        sequence_shape_stridedslice_stack.set_outputs(['root'])

        self.sequences = [sequence_reshape, sequence_flatten, sequence_shape_concat, sequence_shape_stridedslice_stack]

    @staticmethod
    def _is_const_input(graph_helper, input_tensor, tensor):
        """ This function determines a const tensor is an input to reshape and not the shape input"""

        if input_tensor.op.type != 'Const':
            return False

        input_tensor_value = graph_helper.evaluate_tensor_output(input_tensor).tolist()
        tensor_shape = graph_helper.get_op_output_shape(tensor)
        # its a shape input if -1 or 0 in value or if the output of reshape is the same as the value of the
        # the shape tensor
        if tensor_shape == input_tensor_value or (type(input_tensor_value) is list and
                                                      (-1 in input_tensor_value or 0 in input_tensor_value)):
            return False
        return True

    @staticmethod
    def _get_const_descriptor(graph_helper, input_tensor, consumed_nodes, consumer_descriptor):
        const_value = graph_helper.evaluate_tensor_output(input_tensor)
        const_shape = graph_helper.get_op_output_shape(input_tensor)
        return ConstantLayerResolver.Descriptor(str(input_tensor.op.name),
                                                consumed_nodes,
                                                const_value,
                                                const_shape,
                                                consumer_descriptor)

    def resolve_layer(self, graph_matcher, graph_helper):
        descriptors = []
        for sequence in self.sequences:
            matches = graph_matcher.match_sequence(sequence)
            for match in matches:
                reshape_op = match['root']
                consumed_nodes = match.consumed_nodes
                reshape_descriptor = ReshapeLayerResolver.Descriptor(str(reshape_op.name), consumed_nodes,
                                                                     reshape_op)
                descriptors.append(reshape_descriptor)
                # create constant descriptor for static input to reshape. This is will later be used in transform layer
                # to squash reshape. Note: we are not squashing here since we do not have the output descriptor yet.
                if reshape_op.type == "Reshape":
                    for reshape_input in reshape_op.inputs:
                        input_, consumed_nodes = graph_helper.get_none_identity_input(reshape_input)
                        if self._is_const_input(graph_helper, input_, reshape_op.outputs[0]):
                            descriptors.append(self._get_const_descriptor(graph_helper, input_, consumed_nodes,
                                                                          reshape_descriptor))
        return descriptors


class ReshapeLayerBuilder(LayerBuilder):
    def build_layer(self, ir_graph, converter_context, descriptor, input_descriptors, output_descriptors):
        """
        :type ir_graph: converters.common.converter_ir.op_graph.IROpGraph
        :type input_descriptors: [converters.tensorflow.common.LayerDescriptor]
        :type output_descriptors: [converters.tensorflow.common.LayerDescriptor]
        :type converter_context: converters.tensorflow.converter.ConverterContext
        :type descriptor: ReshapeLayerResolver.Descriptor
        :rtype: int
        """
        input_name = self.get_input_name(converter_context, descriptor, input_descriptors[:1])
        output_shape = converter_context.graph_helper.get_op_output_shape(descriptor.reshape_op)
        output_shape = output_shape[-4:] if len(output_shape) > 4 else output_shape
        return ir_graph.add(ReshapeOp(descriptor.output_names[0],
                                      output_shape),
                            input_names=input_name,
                            output_names=descriptor.output_names[0])


    def transform_layer(self, ir_graph, converter_context, descriptor, input_descriptors, output_descriptors):
        fc_outputs = [d for d in output_descriptors if isinstance(d, FullyConnectedLayerResolver.Descriptor)]
        if len(output_descriptors) == 1 and fc_outputs == output_descriptors:
            # Only make this optimization if the batch dimension is maintained through the reshape.
            # If not maintained, then reshape op is needed.
            non_ignored_inputs = [d for d in input_descriptors if not d.is_ignored]
            if len(non_ignored_inputs) == 1:
                tensors = converter_context.get_output_tensors_between(non_ignored_inputs[0],
                                                                       descriptor)
                input_batch = converter_context.graph_helper.get_op_output_shape(tensors[0].op)[0]
                output_batch = converter_context.graph_helper.get_op_output_shape(
                    descriptor.reshape_op)[0]
                if input_batch == output_batch:
                    converter_context.merge_descriptors(descriptor, fc_outputs[0])
                    return

        non_ignored_inputs = [d for d in input_descriptors if not d.is_ignored]
        if len(non_ignored_inputs) == 1:
            tensors = converter_context.get_output_tensors_between(non_ignored_inputs[0], descriptor)
            input_shape = converter_context.graph_helper.get_op_output_shape(tensors[0].op)
            output_shape = converter_context.graph_helper.get_op_output_shape(descriptor.child_ops[0])
            if input_shape == output_shape:
                converter_context.merge_descriptors(descriptor, non_ignored_inputs[0])
            elif isinstance(non_ignored_inputs[0], ConstantLayerResolver.Descriptor):
                const_desc = non_ignored_inputs[0]
                const_desc.value = np.reshape(const_desc.value, output_shape)
                const_desc.shape = output_shape
                converter_context.merge_descriptors(descriptor, const_desc)
        elif len(non_ignored_inputs) == 0:
            # only set descriptor as ignored if there no inputs to ignored op
            if all(len(d.child_ops[0].inputs) == 0 for d in input_descriptors):
                descriptor.set_ignored(True)
