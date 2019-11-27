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
 * Copyright (c) 2018, Open AI Lab
 * Author: chunyinglv@openailab.com
 */

#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <string.h>
#include <vector>
#include <stdio.h>
#include "tengine_c_api.h"
#include "tengine_operations.h"
#include <sys/time.h>
#include "common.hpp"

#define DEF_PROTO "models/MobileNetSSD_deploy.prototxt"
#define DEF_MODEL "models/MobileNetSSD_deploy.caffemodel"
#define DEF_IMAGE "tests/images/ssd_dog.jpg"

struct Box
{
    float x0;
    float y0;
    float x1;
    float y1;
    int class_idx;
    float score;
};

void get_input_data_ssd(std::string& image_file, float* input_data, int img_h, int img_w)
{
    float mean[3] = {127.5, 127.5, 127.5};
    float scales[3] = {0.007843, 0.007843, 0.007843};
    image img = imread(image_file.c_str(), img_w, img_h, mean, scales, CAFFE);    
    memcpy(input_data, img.data, sizeof(float)*3*img_w*img_h);  
    free_image(img);
}

void post_process_ssd(std::string& image_file, float threshold, float* outdata, int num, std::string& save_name)
{
    const char* class_names[] = {"background", "aeroplane", "bicycle",   "bird",   "boat",        "bottle",
                                 "bus",        "car",       "cat",       "chair",  "cow",         "diningtable",
                                 "dog",        "horse",     "motorbike", "person", "pottedplant", "sheep",
                                 "sofa",       "train",     "tvmonitor"};

    image im = imread(image_file.c_str());

    int raw_h = im.h;
    int raw_w = im.w;
    std::vector<Box> boxes;
    printf("detect result num: %d \n", num);
    for(int i = 0; i < num; i++)
    {
        if(outdata[1] >= threshold)
        {
            Box box;
            box.class_idx = outdata[0];
            box.score = outdata[1];
            box.x0 = outdata[2] * raw_w;
            box.y0 = outdata[3] * raw_h;
            box.x1 = outdata[4] * raw_w;
            box.y1 = outdata[5] * raw_h;
            boxes.push_back(box);
            printf("%s\t:%.0f%%\n", class_names[box.class_idx], box.score * 100);
            printf("BOX:( %g , %g ),( %g , %g )\n", box.x0, box.y0, box.x1, box.y1);
        }
        outdata += 6;
    }
    for(int i = 0; i < ( int )boxes.size(); i++)
    {
        Box box = boxes[i];

        std::ostringstream score_str;
        score_str << box.score * 100;
        std::string labelstr = std::string(class_names[box.class_idx]) + " : " + score_str.str();

        put_label(im, labelstr.c_str(), 0.02, box.x0, box.y0, 255, 255, 125);
        draw_box(im, box.x0, box.y0, box.x1, box.y1, 2, 125, 0, 125);
    }

    save_image(im, "Mobilenet_SSD.jpg");
    free_image(im);
    std::cout << "======================================\n";
    std::cout << "[DETECTED IMAGE SAVED]:\t"
              << "Mobilenet_SSD"
              << "\n";
    std::cout << "======================================\n";
}

int main(int argc, char* argv[])
{
    int ret = -1;
    const std::string root_path = get_root_path();
    std::string proto_file;
    std::string model_file;
    std::string image_file;
    std::string save_name = "save.jpg";
    const char* device = nullptr;

    int res;
    while((res = getopt(argc, argv, "p:m:i:hd:")) != -1)
    {
        switch(res)
        {
            case 'p':
                proto_file = optarg;
                break;
            case 'm':
                model_file = optarg;
                break;
            case 'i':
                image_file = optarg;
                break;
            case 'd':
                device = optarg;
                break;
            case 'h':
                std::cout << "[Usage]: " << argv[0] << " [-h]\n"
                          << "   [-p proto_file] [-m model_file] [-i image_file]\n";
                return 0;
            default:
                break;
        }
    }

    if(proto_file.empty())
    {
        proto_file = root_path + DEF_PROTO;
        std::cout << "proto file not specified,using " << proto_file << " by default\n";
    }
    if(model_file.empty())
    {
        model_file = root_path + DEF_MODEL;
        std::cout << "model file not specified,using " << model_file << " by default\n";
    }
    if(image_file.empty())
    {
        image_file = root_path + DEF_IMAGE;
        std::cout << "image file not specified,using " << image_file << " by default\n";
    }

    // init tengine
    if(init_tengine() < 0)
    {
        std::cout << " init tengine failed\n";
        return 1;
    }
    if(request_tengine_version("0.9") != 1)
    {
        std::cout << " request tengine version failed\n";
        return 1;
    }
    // check file
    if(!check_file_exist(proto_file) or (!check_file_exist(model_file) or !check_file_exist(image_file)))
    {
        return 1;
    }
    // create graph
    graph_t graph = create_graph(nullptr, "caffe", proto_file.c_str(), model_file.c_str());

    if(graph == nullptr)
    {
        std::cout << "Create graph failed\n";
        std::cout << " ,errno: " << get_tengine_errno() << "\n";
        return 1;
    }

    if(device != nullptr)
    {
        set_graph_device(graph, device);
    }

    // dump_graph(graph);

    // input
    int img_h = 300;
    int img_w = 300;
    int img_size = img_h * img_w * 3;
    float* input_data = ( float* )malloc(sizeof(float) * img_size);

    int node_idx = 0;
    int tensor_idx = 0;
    tensor_t input_tensor = get_graph_input_tensor(graph, node_idx, tensor_idx);
    if(input_tensor == nullptr)
    {
        std::printf("Cannot find input tensor,node_idx: %d,tensor_idx: %d\n", node_idx, tensor_idx);
        return -1;
    }

    int dims[] = {1, 3, img_h, img_w};
    set_tensor_shape(input_tensor, dims, 4);
    ret = prerun_graph(graph);
    if(ret != 0)
    {
        std::cout << "Prerun graph failed, errno: " << get_tengine_errno() << "\n";
        return 1;
    }

    int repeat_count = 1;
    const char* repeat = std::getenv("REPEAT_COUNT");

    if(repeat)
        repeat_count = std::strtoul(repeat, NULL, 10);

    // warm up
    get_input_data_ssd(image_file, input_data, img_h, img_w);
    set_tensor_buffer(input_tensor, input_data, img_size * 4);
    ret = run_graph(graph, 1);
    if(ret != 0)
    {
        std::cout << "Run graph failed, errno: " << get_tengine_errno() << "\n";
        return 1;
    }

    struct timeval t0, t1;
    float total_time = 0.f;
    for(int i = 0; i < repeat_count; i++)
    {
        get_input_data_ssd(image_file, input_data, img_h, img_w);

        gettimeofday(&t0, NULL);
        run_graph(graph, 1);
        gettimeofday(&t1, NULL);
        float mytime = ( float )((t1.tv_sec * 1000000 + t1.tv_usec) - (t0.tv_sec * 1000000 + t0.tv_usec)) / 1000;
        total_time += mytime;
    }
    std::cout << "--------------------------------------\n";
    std::cout << "repeat " << repeat_count << " times, avg time per run is " << total_time / repeat_count << " ms\n";
    tensor_t out_tensor = get_graph_output_tensor(graph, 0, 0);    //"detection_out");
    int out_dim[4];
    ret = get_tensor_shape(out_tensor, out_dim, 4);
    if(ret <= 0)
    {
        std::cout << "get tensor shape failed, errno: " << get_tengine_errno() << "\n";
        return 1;
    }
    float* outdata = ( float* )get_tensor_buffer(out_tensor);
    int num = out_dim[1];
    float show_threshold = 0.5;

    post_process_ssd(image_file, show_threshold, outdata, num, save_name);

    release_graph_tensor(out_tensor);
    release_graph_tensor(input_tensor);

    ret = postrun_graph(graph);
    if(ret != 0)
    {
        std::cout << "Postrun graph failed, errno: " << get_tengine_errno() << "\n";
        return 1;
    }
    free(input_data);
    destroy_graph(graph);
    release_tengine();

    return 0;
}
