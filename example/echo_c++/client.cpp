// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// A client sending requests to server every 1 second.

#include <gflags/gflags.h>
#include <butil/logging.h>
#include <butil/time.h>
#include <brpc/channel.h>
#include "echo.pb.h"

DEFINE_string(attachment, "", "Carry this along with requests");

// 初始化Channel需要的参数
DEFINE_string(protocol, "baidu_std", "Protocol type. Defined in src/brpc/options.proto");
DEFINE_string(connection_type, "", "Connection type. Available values: single, pooled, short");
DEFINE_string(server, "0.0.0.0:8000", "IP Address of server");
DEFINE_string(load_balancer, "", "The algorithm for load balancing");
DEFINE_int32(timeout_ms, 100, "RPC timeout in milliseconds");
DEFINE_int32(max_retry, 3, "Max retries(not including the first RPC)"); 

// 连续请求之间的间隔时间
DEFINE_int32(interval_ms, 1000, "Milliseconds between consecutive requests");

int main(int argc, char* argv[]) {
    // Parse gflags. We recommend you to use gflags as well.
    // 解析gflags. 我们建议你也使用gflag
    GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);
    
    // A Channel represents a communication line to a Server. Notice that 
    // Channel is thread-safe and can be shared by all threads in your program.

    // 1个Channel代表和server通信的管道. 注意Channel是线程安全的, 可以被程序中的所有线程共享
    // Channel是线程安全的代表同1个下游可以全局使用相同的Channel对象
    // 在会访问其他下游的rpc服务中特别方便, 不同的rpc请求在不同的bthread中, 可以避免重复建立连接
    brpc::Channel channel;
    
    // Initialize the channel, NULL means using default options.
    // 初始化channel, NULL代表使用默认值, Init结束后不会再访问options, 可以放到栈上
    // Channel.options()可以获取到当前channel使用的所有选项
    brpc::ChannelOptions options;
    options.protocol = FLAGS_protocol;                      // 协议
    options.connection_type = FLAGS_connection_type;        // 连接类型
    options.timeout_ms = FLAGS_timeout_ms/*milliseconds*/;  // 超时时间
    options.max_retry = FLAGS_max_retry;                    // 重试次数(不包含第1次rpc请求)

    // TODO: FLAGS_server为单台&FLAGS_load_balancer不为空时, 此时如何处理呢?
    if (channel.Init(FLAGS_server.c_str(), FLAGS_load_balancer.c_str(), &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        return -1;
    }

    // Normally, you should not call a Channel directly, but instead construct
    // a stub Service wrapping it. stub can be shared by all threads as well.

    // 通常你不需要直接调用Channle对象, 而是构造1个xxx_Stub进行间接访问. 同时stub也是线程安全的
    // stub内没什么成员变量, 建议在栈上创建和使用, 而不必new, 当然也可以把stub存下来复用
    example::EchoService_Stub stub(&channel);

    // Send a request and wait for the response every 1 second.
    // 每秒发送1个请求并同步等待响应
    int log_id = 0;
    while (!brpc::IsAskedToQuit()) {
        // We will receive response synchronously, safe to put variables
        // on stack.
        // 通过同步的方式接收响应, 可以将变量放到栈上
        example::EchoRequest request;
        example::EchoResponse response;
        brpc::Controller cntl;

        request.set_message("hello world");

        cntl.set_log_id(log_id ++);  // set by user
        // Set attachment which is wired to network directly instead of 
        // being serialized into protobuf messages.

        // 设置通过网络直接传输的附件而不是序列化的proto message
        cntl.request_attachment().append(FLAGS_attachment);

        // Because `done'(last parameter) is NULL, this function waits until
        // the response comes back or error occurs(including timedout).

        // 因为done是NULL, Echo函数一直等待响应返回或者发生错误
        stub.Echo(&cntl, &request, &response, NULL);
        if (!cntl.Failed()) {
            LOG(INFO) << "Received response from " << cntl.remote_side()
                << " to " << cntl.local_side()
                << ": " << response.message() << " (attached="
                << cntl.response_attachment() << ")"
                << " latency=" << cntl.latency_us() << "us";
        } else {
            LOG(WARNING) << cntl.ErrorText();
        }
        usleep(FLAGS_interval_ms * 1000L);
    }

    LOG(INFO) << "EchoClient is going to quit";
    return 0;
}
