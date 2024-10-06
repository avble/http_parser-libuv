/*
/* Copyright (c) 2024-2024 Harry Le (avble.harry at gmail dot com)

It can be used, modified.
*/

#pragma once

#include <http_parser.h>
#include <uv.h>

#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>

#define MAX_WRITE_HANDLES 21000000

class Event
{

public:
    template <class F, class... Args>
    static void call_soon(F f, Args... args)
    {
        struct wrapper_
        {
            wrapper_(F f, Args... args) : func(std::bind(f, args...)) {}
            void operator()() { func(); }
            std::function<void()> func;
        };
        static uv_timer_cb func_call_01 = [](uv_timer_t * handle) {
            wrapper_ * p = (wrapper_ *) (handle->data);
            (*p)();
            delete p;
            free(handle);
        };
        uv_timer_t * timer_handle = (uv_timer_t *) malloc(sizeof(uv_timer_t));
        timer_handle->data        = (void *) (new wrapper_(std::forward<F>(f), std::forward<Args>(args)...));
        uv_timer_init(uv_default_loop(), timer_handle);
        uv_timer_start(timer_handle, func_call_01, 0, 0);
    }
};

namespace http {

class response
{
public:
    response() {}
    response(const response & other) { body_ = other.body_; }
    response(const response && other) { body_ = other.body_; }
    std::string & body() { return body_; }

private:
    std::string body_;
};

void start_server(unsigned short port, std::function<void(response &)> _handler)
{
    class http_session : public std::enable_shared_from_this<http_session>
    {
        struct internal_wrapper
        {
            internal_wrapper(std::shared_ptr<http_session> _p) { p_session = _p; }

            internal_wrapper(const internal_wrapper & other) { p_session = other.p_session; }

            std::shared_ptr<http_session> p_session;
        };

    public:
        http_session(std::function<void(response &)> _handler)
        {
            handler = _handler;
            uv_tcp_init(uv_default_loop(), &tcp_stream);
            std::memset(&settings, 0, sizeof settings);
        }

        ~http_session()
        {
            static uv_close_cb close_cb_ = [](uv_handle_t * handle) {

            };
            uv_close((uv_handle_t *) &tcp_stream, close_cb_);
        }

        void start()
        {
            // std::cout << "[DEBUG][http_session][start] " << std::endl;
            tcp_stream.data = (void *) (new internal_wrapper(shared_from_this()));
            do_read_request();
        }

    private:
        http_session()                                  = delete;
        http_session(const http_session &)              = delete;
        http_session(const http_session &&)             = delete;
        http_session & operator=(const http_session &)  = delete;
        http_session & operator=(const http_session &&) = delete;

    public:
        void do_read_request()
        {
            // std::cout << "[DEBUG][http_session][do_read_request] " << std::endl;
            static http_cb llhttp_on_message = [](http_parser * req) -> int {
                internal_wrapper * wrapper_ = (internal_wrapper *) req->data;
                auto self                   = wrapper_->p_session;

                response res;
                self->handler(res);

                auto do_write_ = [func_ = std::bind(&http_session::do_write_response, self, res)]() { func_(); };
                Event::call_soon(do_write_);

                return 0;
            };

            static uv_read_cb read_cb = [](uv_stream_t * tcp, ssize_t nread, const uv_buf_t * buf) {
                // std::cout << "[DEBUG][http_session][read_cb] " << std::endl;
                if (nread > 0)
                {
                    internal_wrapper * wrapper_ = (internal_wrapper *) tcp->data;
                    auto self                   = wrapper_->p_session;

                    size_t return_size = http_parser_execute(&(self->parser), &(self->settings), buf->base, nread);
                    if (return_size != nread)
                        std::cout << "[DEBUG] need to handle this case\n";
                    free(buf->base);
                }
                else if (nread <= 0)
                {
                    internal_wrapper * p = (internal_wrapper *) tcp->data;
                    p->p_session->abort();
                }
            };

            int rc = uv_read_start((uv_stream_t *) &tcp_stream,
                                   [](uv_handle_t * handle, size_t suggested_size, uv_buf_t * buf) {
                                       *buf = uv_buf_init((char *) malloc(suggested_size), suggested_size);
                                   },
                                   [](uv_stream_t * tcp, ssize_t nread, const uv_buf_t * buf) { read_cb(tcp, nread, buf); });

            if (rc != 0)
                std::cout << "error: " << rc << std::endl;

            settings.on_message_complete = llhttp_on_message;

            http_parser_init(&parser, HTTP_BOTH);
            parser.data = tcp_stream.data;
        }

        void do_write_response(response res)
        {
            auto do_write_ = [self = shared_from_this()](response res) {
                std::string header = "HTTP/1.0 200 OK";
                header += "\n";
                header += "Connection: keep-alive";
                header += "\n";
                header += "Content-Length: ";
                header += std::to_string(res.body().size());
                header += "\r\n\r\n";
                header += res.body();

                uv_buf_t resbuf;
                resbuf.base = (char *) header.c_str();
                resbuf.len  = header.size();
                uv_write(&self->write_res, (uv_stream_t *) &self->tcp_stream, &resbuf, 1, [](uv_write_t * writer, int status) {
                    uv_tcp_t * tcp_stream_ = (uv_tcp_t *) writer->handle;
                    internal_wrapper * p   = (internal_wrapper *) tcp_stream_->data;
                    p->p_session->on_write(status);
                });
            };

            Event::call_soon(do_write_, res);
        }

        void on_write(int status)
        {
            if (status != 0)
                abort();
        }

        void abort()
        {
            uv_close((uv_handle_t *) &tcp_stream, [](uv_handle_t * handle) {
                internal_wrapper * p = (internal_wrapper *) handle->data;
                p->p_session.reset();
                delete p;
            });
        }

    public:
        uv_tcp_t tcp_stream;
        uv_write_t write_res;
        http_parser parser;
        http_parser_settings settings;
        std::function<void(response &)> handler;
    };

    static auto on_connect = [&](uv_stream_t * handle, int status) {
        std::tuple<std::function<void(response &)>> * p = (std::tuple<std::function<void(response &)>> *) handle->data;

        auto http_connection = std::shared_ptr<http_session>{ new http_session(std::get<0>(*p)), [](http_session * p) {
                                                                 //    std::cout << "[DEBUG] http_session is deleted.\n";
                                                                 delete p;
                                                             } };

        int rc = uv_accept(handle, (uv_stream_t *) &http_connection->tcp_stream);
        if (rc < 0)
            std::cout << "[DEBUG] error: " << rc << std::endl;

        http_connection->start();
    };

    uv_tcp_t server;
    int cores = sysconf(_SC_NPROCESSORS_ONLN);
    char cores_string[10];
    sprintf(cores_string, "%d", cores);
    setenv("UV_THREADPOOL_SIZE", cores_string, 1);

    uv_tcp_init(uv_default_loop(), &server);

    struct sockaddr_in address;
    uv_ip4_addr("0.0.0.0", port, &address);
    uv_tcp_bind(&server, (const struct sockaddr *) &address, 0);

    server.data = new std::tuple<decltype(_handler)>(_handler);

    int rc =
        uv_listen((uv_stream_t *) &server, MAX_WRITE_HANDLES, [](uv_stream_t * server, int status) { on_connect(server, status); });

    if (rc >= 0)
    {
        std::cout << "server has started on port: " << port << std::endl;
        uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    }
    else
        std::cout << "server failed at starting on port: " << port << std::endl;
}

} // namespace http
