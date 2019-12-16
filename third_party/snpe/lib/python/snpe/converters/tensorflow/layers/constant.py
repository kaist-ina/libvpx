#!/usr/bin/env python
# =============================================================================
#
#  Copyright (c) 2015-2019 Qualcomm Technologies, Inc.
#  All Rights Reserved.
#  Confidential and Proprietary - Qualcomm Technologies, Inc.
#
# =============================================================================
import numpy as np
from snpe.converters.common.converter_ir.axis_tracker import AxisTracker
from snpe.converters.common.converter_ir.op_adapter import ConstantOp
from snpe.converters.tensorflow.common import LayerDescriptor, LayerResolver, LayerBuilder
from snpe.converters.tensorflow.layers.ignored_patterns import IgnoredLayersResolver
from snpe.converters.tensorflow.util import ConverterError


class ConstantLayerResolver(LayerResolver, object):
    def resolve_layer(self, graph_matcher, graph_helper):
        raise ConverterError('Constant layers are resolved by other resolvers!')

    class Descriptor(LayerDescriptor):
        def __init__(self, name, nodes, value, shape, consumer):
            super(ConstantLayerResolver.Descriptor, self).__init__('Constant', name, nodes)
            self.value = value
            self.shape = shape
            self.consumer = consumer

        def is_input_tensor(self, op, tensor):
            return False


class ConstantLayerBuilder(LayerBuilder):

    def transform_layer(self, ir_graph, converter_context, descriptor, input_descriptors, output_descriptors):
        ignored = [d for d in output_descriptors if isinstance(d, IgnoredLayersResolver.Descriptor)]
        if ignored == output_descriptors:
            descriptor.set_ignored(True)

        if len(output_descriptors) == 1 and not descriptor.consumer == output_descriptors[0]:
            descriptor.set_ignored(True)

    def build_layer(self, ir_graph, converter_context, descriptor, input_descriptors, output_descriptors):
        """
        :type ir_graph: converters.common.converter_ir.op_graph.IROpGraph
        :type input_descriptors: [converters.tensorflow.common.LayerDescriptor]
        :type output_descriptors: [converters.tensorflow.common.LayerDescriptor]
        :type converter_context: converters.tensorflow.converter.ConverterContext
        :type descriptor: ConstantLayerResolver.Descriptor
        :rtype: int
        """

        # ConstantOp has no inputs
        input_names = []

        if not isinstance(descriptor.value, np.ndarray):
            array = np.zeros(descriptor.shape, dtype=np.float32)
            array[...] = descriptor.value
            descriptor.value = array

        return ir_graph.add(ConstantOp(descriptor.output_names[0],
                                       descriptor.value),
                            input_names,
                            descriptor.output_names[0])
