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

// A server to receive EchoRequest and send back EchoResponse.

#include <gflags/gflags.h>
#include <butil/logging.h>
#include <brpc/server.h>
#include "echo.pb.h"

DEFINE_bool(echo_attachment, true, "Echo attachment as well");

// 指定监听地址, 优先级listen_addr => port
DEFINE_int32(port, 8000, "TCP Port of this server");
DEFINE_string(listen_addr, "", "Server listen address, may be IPV4/IPV6/UDS."
            " If this is set, the flag port will be ignored");

// 在idle_timeout_s时间内没有进行读、写操作, 连接将会被关闭, 该值为-1表示不会主动关闭
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");
DEFINE_int32(logoff_ms, 2000, "Maximum duration of server's LOGOFF state "
             "(waiting for client to close connection before server stops)");

// Your implementation of example::EchoService
// Notice that implementing brpc::Describable grants the ability to put
// additional information in /status.

// 对example::EchoService自定义实现
// 注意对brpc::Describable的实现确保可以将附加信息放入到/status中
// 实现具体类通常是在后面加上Impl后缀, 比如xxxService, 其具体类为xxServiceImpl
namespace example {
class EchoServiceImpl : public EchoService {
public:
    // 构造函数和析构函数使用默认即可
    EchoServiceImpl() {};
    virtual ~EchoServiceImpl() {};
    // 对Echo接口的具体实现
    virtual void Echo(google::protobuf::RpcController* cntl_base,
                      const EchoRequest* request,
                      EchoResponse* response,
                      google::protobuf::Closure* done) {
        // This object helps you to call done->Run() in RAII style. If you need
        // to process the request asynchronously, pass done_guard.release().

        // brpc::ClosureGuard使用RAII的方式确保done->Run()函数会被调用
        // 如果需要异步处理, 需要调用done_guard.release()释放
        // done中包含了调用Echo回调后的后续动作, 包括response正确性、序列化、打包、发送等逻辑
        brpc::ClosureGuard done_guard(done);

        // 运行在brpc::Server中时可以将RpcController静态转换为brpc::Controller
        // 其中brpc::Controller包含此次rpc请求中相关信息, 不限于remote ip, logid等信息
        brpc::Controller* cntl =
            static_cast<brpc::Controller*>(cntl_base);

        // The purpose of following logs is to help you to understand
        // how clients interact with servers more intuitively. You should 
        // remove these logs in performance-sensitive servers.

        // 以下日志的目的是帮助你更直观的了解client和server的交互方式
        // 你应该在性能敏感的服务器中删除这些日志
        // Controller中包含logid, 但在实际场景中更多将logid放到proto message字段中
        LOG(INFO) << "Received request[log_id=" << cntl->log_id() 
                  << "] from " << cntl->remote_side() 
                  << " to " << cntl->local_side()
                  << ": " << request->message()
                  << " (attached=" << cntl->request_attachment() << ")";

        // Fill response.
        // 填充response信息
        response->set_message(request->message());

        // You can compress the response by setting Controller, but be aware
        // that compression may be costly, evaluate before turning on.
        // cntl->set_response_compress_type(brpc::COMPRESS_TYPE_GZIP);

        // 你可以通过设置Controller来压缩响应, 但是需要注意压缩可能是非常耗时的, 在打开前进行评估:
        // 打开方式为: cntl->set_response_compress_type(brpc::COMPRESS_TYPE_GZIP)
        if (FLAGS_echo_attachment) { // 将附加也进行回传
            // Set attachment which is wired to network directly instead of
            // being serialized into protobuf messages.

            // 设置通过网络直接传输的附件而不是序列化的proto message
            cntl->response_attachment().append(cntl->request_attachment());
        }
    }
};
}  // namespace example

int main(int argc, char* argv[]) {
    // Parse gflags. We recommend you to use gflags as well.
    // 解析gflags. 我们建议你也使用gflag
    GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);

    // Generally you only need one Server.
    // 通常你只需要1个server, 即1个进程使用1个server, 监控1个端口
    brpc::Server server;

    // Instance of your service.
    // echo服务实例, 全局只需要1个即可
    example::EchoServiceImpl echo_service_impl;

    // Add the service into server. Notice the second parameter, because the
    // service is put on stack, we don't want server to delete it, otherwise
    // use brpc::SERVER_OWNS_SERVICE.

    // 将服务添加到server中. 注意第二参数, 因为serivce实例是在栈上
    // 我们不期望server去delete, 否则需要使用brpc::SERVER_OWNS_SERVICE参数
    // 同时server中对AddService进行了多个重载实现, 可以注册单个接口到server中
    // 或者通过ServiceOptions的restful_mappings进行指定
    if (server.AddService(&echo_service_impl, 
                          brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add service";
        return -1;
    }

    // 构造地址信息
    butil::EndPoint point;
    if (!FLAGS_listen_addr.empty()) {
        if (butil::str2endpoint(FLAGS_listen_addr.c_str(), &point) < 0) {
            LOG(ERROR) << "Invalid listen address:" << FLAGS_listen_addr;
            return -1;
        }
    } else {
        // 以端口的方式指定地址
        point = butil::EndPoint(butil::IP_ANY, FLAGS_port);
    }

    // Start the server.
    brpc::ServerOptions options;
    options.idle_timeout_sec = FLAGS_idle_timeout_s; // 指定断开链接的超时时间
    if (server.Start(point, &options) != 0) {
        LOG(ERROR) << "Fail to start EchoServer";
        return -1;
    }

    // Wait until Ctrl-C is pressed, then Stop() and Join() the server.
    // 阻塞直到按下Ctrl-C, 然后依次调用Stop()和Join()函数
    server.RunUntilAskedToQuit();
    return 0;
}
