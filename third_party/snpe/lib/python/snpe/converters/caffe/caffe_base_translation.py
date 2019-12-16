# ==============================================================================
#
#  Copyright (c) 2019 Qualcomm Technologies, Inc.
#  All Rights Reserved.
#  Confidential and Proprietary - Qualcomm Technologies, Inc.
#
# ==============================================================================

from snpe.converters.common.converter_ir import translation

# ------------------------------------------------------------------------------
#   CaffeTranslation
# ------------------------------------------------------------------------------
CaffeTranslations = translation.TranslationBank()


class CaffeTranslationBase(translation.ConversionTranslationBase):
    def __init__(self):
        translation.ConversionTranslationBase.__init__(self)

    def extract_parameters(self, src_op, graph):
        raise NotImplementedError("extract_parameters for {} not implemented ".format(str(self.__class__.__name__)))

    def extract_input_names(self, src_op, graph):
        return list(map(str, src_op.bottom))

    def extract_output_names(self, src_op, graph):
        return list(map(str, src_op.top))
