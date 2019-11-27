/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * License); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * AS IS BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * Copyright (c) 2019, Open AI Lab
 * Author: chunyinglv@openailab.com
 */
#include <iostream>
#include <functional>
#include <cstring>
#include <algorithm>

#include "logger.hpp"
#include "node_ops.hpp"
#include "tensor_mem.hpp"
#include "graph.hpp"
#include "operator/pad.hpp"
#include "data_type.hpp"
#include "compiler_fp16.h"

namespace TEngine {

namespace PadImpl {

struct PadOps : public NodeOps
{
    template <typename T>
    void kernel_run(T* input, T* output, int batch_num, int in_c, int in_h, int in_w, int out_c, int out_h, int out_w,
                    PadParam* param)
    {
        int out_chw = out_c * out_h * out_w;
        int out_hw = out_h * out_w;
        int in_chw = in_c * in_h * in_w;
        int in_hw = in_h * in_w;
        memset(output, 0, sizeof(T) * out_chw * batch_num);
        for(int n = 0; n < batch_num; n++)
        {
            for(int c = 0; c < in_c; c++)
            {
                for(int h = 0; h < in_h; h++)
                {
                    for(int w = 0; w < in_w; w++)
                    {
                        output[(param->pad_0_h + n) * out_chw + (param->pad_1_h + c) * out_hw +
                               (param->pad_2_h + h) * out_w + (param->pad_3_h + w)] =
                            input[n * in_chw + c * in_hw + h * in_w + w];
                    }
                }
            }
        }
    }
    bool Run(Node* node) override
    {
        Tensor* input_tensor = node->GetInputTensor(0);
        Tensor* output_tensor = node->GetOutputTensor(0);

        // float * input=(float *)get_tensor_mem(input_tensor);
        // float * output=(float *)get_tensor_mem(output_tensor);

        Pad* pad_op = dynamic_cast<Pad*>(node->GetOp());
        PadParam* param = pad_op->GetParam();

        const TShape& shape = input_tensor->GetShape();
        const std::vector<int>& in_dim = shape.GetDim();

        const TShape& shape1 = output_tensor->GetShape();
        const std::vector<int>& out_dim = shape1.GetDim();

#if 0
        float * input=(float *)get_tensor_mem(input_tensor);
        float * output=(float *)get_tensor_mem(output_tensor);
        int out_w   = out_dim[3];
        int out_hw  = out_dim[2] * out_w;
        int out_chw = out_dim[1] * out_hw;

        int in_w   = in_dim[3];
        int in_hw  = in_dim[2] * in_w;
        int in_chw = in_dim[1] * in_hw;

        memset(output,0,sizeof(float)*out_chw*out_dim[0]);

        for(int n=0;n<in_dim[0];n++)
        {
            for(int c=0;c<in_dim[1];c++)
            {
                for(int h=0;h<in_dim[2];h++)
                {
                    for(int w=0;w<in_dim[3];w++)
                    {
                        output[(param->pad_0_h + n)*out_chw +
                               (param->pad_1_h + c)*out_hw +
                               (param->pad_2_h + h)*out_w +
                               (param->pad_3_h + w) ] = input[n*in_chw + c*in_hw + h*in_w + w];
                    }
                }
            }
        }
#else

        int element_size = DataType::GetTypeSize(input_tensor->GetDataType());
        if(1 == element_size)
        {
            int8_t* input = ( int8_t* )get_tensor_mem(input_tensor);
            int8_t* output = ( int8_t* )get_tensor_mem(output_tensor);
            kernel_run<int8_t>(input, output, in_dim[0], in_dim[1], in_dim[2], in_dim[3], out_dim[1], out_dim[2],
                               out_dim[3], param);
            auto* in_quant = input_tensor->GetQuantParam();
            auto* out_quant = output_tensor->GetQuantParam();
            QuantParam q_param;
            q_param.scale = (*in_quant)[0].scale;
            q_param.zero_point = 0;
            out_quant->resize(0);
            out_quant->push_back(q_param);
        }
        if(2 == element_size)
        {
            __fp16* input = ( __fp16* )get_tensor_mem(input_tensor);
            __fp16* output = ( __fp16* )get_tensor_mem(output_tensor);
            kernel_run<__fp16>(input, output, in_dim[0], in_dim[1], in_dim[2], in_dim[3], out_dim[1], out_dim[2],
                               out_dim[3], param);
        }
        if(4 == element_size)
        {
            float* input = ( float* )get_tensor_mem(input_tensor);
            float* output = ( float* )get_tensor_mem(output_tensor);
            kernel_run<float>(input, output, in_dim[0], in_dim[1], in_dim[2], in_dim[3], out_dim[1], out_dim[2],
                              out_dim[3], param);
        }

#endif
        return true;
    }
};

NodeOps* SelectFunc(const CPUInfo* cpu_info, Node* node)
{
    Tensor* input = node->GetInputTensor(0);
    const int data_type = input->GetDataType();
    const ExecAttr* exec_attr = any_cast<const ExecAttr*>(node->GetAttr(ATTR_EXEC_ATTR));
    if((data_type != TENGINE_DT_FP32 && data_type != TENGINE_DT_FP16 && data_type != TENGINE_DT_INT8) ||
       exec_attr->graph_layout != TENGINE_LAYOUT_NCHW)
        return nullptr;

    PadOps* ops = new PadOps();

    return ops;
}

}    // namespace PadImpl

using namespace PadImpl;

void RegisterPadNodeExec(void)
{
    NodeOpsRegistryManager::RegisterOPImplementor("common", "Pad", PadImpl::SelectFunc, 1000);
}

}    // namespace TEngine
