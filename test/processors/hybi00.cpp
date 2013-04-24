/*
 * Copyright (c) 2011, Peter Thorson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the WebSocket++ Project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL PETER THORSON BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
//#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE hybi_00_processor
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/processors/hybi00.hpp>
#include <websocketpp/http/request.hpp>
#include <websocketpp/http/response.hpp>
#include <websocketpp/message_buffer/message.hpp>
#include <websocketpp/message_buffer/alloc.hpp>

struct stub_config {
	typedef websocketpp::http::parser::request request_type;
	typedef websocketpp::http::parser::response response_type;

	typedef websocketpp::message_buffer::message
		<websocketpp::message_buffer::alloc::con_msg_manager> message_type;
	typedef websocketpp::message_buffer::alloc::con_msg_manager<message_type> 
		con_msg_manager_type;
};

struct processor_setup {
    processor_setup(bool server) 
      : msg_manager(new stub_config::con_msg_manager_type())
      , p(false,server,msg_manager) {}
    
    websocketpp::lib::error_code ec;
	stub_config::con_msg_manager_type::ptr msg_manager;
	stub_config::request_type req;
    stub_config::response_type res;
	websocketpp::processor::hybi00<stub_config> p;
};

BOOST_AUTO_TEST_CASE( exact_match ) {
	processor_setup env(true);
    
    std::string handshake = "GET / HTTP/1.1\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nOrigin: http://example.com\r\nSec-WebSocket-Key1: 3e6b263  4 17 80\r\nSec-WebSocket-Key2: 17  9 G`ZD9   2 2b 7X 3 /r90\r\n\r\n";
    
    env.req.consume(handshake.c_str(),handshake.size());
    env.req.replace_header("Sec-WebSocket-Key3","WjN}|M(6");
    
    BOOST_CHECK(websocketpp::processor::is_websocket_handshake(env.req));
    BOOST_CHECK_EQUAL(websocketpp::processor::get_websocket_version(env.req), env.p.get_version());
    env.ec = env.p.validate_handshake(env.req);
    BOOST_CHECK(!env.ec);
    
    websocketpp::uri_ptr u;

    BOOST_CHECK_NO_THROW( u = env.p.get_uri(env.req) );
    
    BOOST_CHECK_EQUAL(u->get_secure(), false);
    BOOST_CHECK_EQUAL(u->get_host(), "www.example.com");
    BOOST_CHECK_EQUAL(u->get_resource(), "/");
    BOOST_CHECK_EQUAL(u->get_port(), websocketpp::URI_DEFAULT_PORT);
    
    env.p.process_handshake(env.req,"",env.res);
    
    BOOST_CHECK_EQUAL(env.res.get_header("Connection"), "Upgrade");
    BOOST_CHECK_EQUAL(env.res.get_header("Upgrade"), "websocket");
    BOOST_CHECK_EQUAL(env.res.get_header("Sec-WebSocket-Origin"), "http://example.com");
        
    BOOST_CHECK_EQUAL(env.res.get_header("Sec-WebSocket-Location"), "ws://www.example.com/");
    BOOST_CHECK_EQUAL(env.res.get_header("Sec-WebSocket-Key3"), "n`9eBk9z$R8pOtVb");
}

BOOST_AUTO_TEST_CASE( non_get_method ) {
	processor_setup env(true);
    
    std::string handshake = "POST / HTTP/1.1\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Key1: 3e6b263  4 17 80\r\nSec-WebSocket-Key2: 17  9 G`ZD9   2 2b 7X 3 /r90\r\n\r\n";
    
    env.req.consume(handshake.c_str(),handshake.size());
    env.req.replace_header("Sec-WebSocket-Key3","janelle!");
    
    BOOST_CHECK(websocketpp::processor::is_websocket_handshake(env.req));
    BOOST_CHECK_EQUAL(websocketpp::processor::get_websocket_version(env.req), env.p.get_version());
    BOOST_CHECK_EQUAL( env.p.validate_handshake(env.req), websocketpp::processor::error::invalid_http_method );
}

BOOST_AUTO_TEST_CASE( old_http_version ) {
	processor_setup env(true);
    
    std::string handshake = "GET / HTTP/1.0\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Key1: 3e6b263  4 17 80\r\nSec-WebSocket-Key2: 17  9 G`ZD9   2 2b 7X 3 /r90\r\n\r\n";
    
    env.req.consume(handshake.c_str(),handshake.size());
    env.req.replace_header("Sec-WebSocket-Key3","janelle!");
    
    BOOST_CHECK(websocketpp::processor::is_websocket_handshake(env.req));
    BOOST_CHECK_EQUAL(websocketpp::processor::get_websocket_version(env.req), env.p.get_version());
    BOOST_CHECK_EQUAL( env.p.validate_handshake(env.req), websocketpp::processor::error::invalid_http_version );
}

BOOST_AUTO_TEST_CASE( missing_handshake_key1 ) {
	processor_setup env(true);
    
    std::string handshake = "GET / HTTP/1.1\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Key1: 3e6b263  4 17 80\r\n\r\n";
    
    env.req.consume(handshake.c_str(),handshake.size());
    env.req.replace_header("Sec-WebSocket-Key3","janelle!");
    
    BOOST_CHECK(websocketpp::processor::is_websocket_handshake(env.req));
    BOOST_CHECK_EQUAL(websocketpp::processor::get_websocket_version(env.req), env.p.get_version());
    BOOST_CHECK_EQUAL( env.p.validate_handshake(env.req), websocketpp::processor::error::missing_required_header );
}

BOOST_AUTO_TEST_CASE( missing_handshake_key2 ) {
	processor_setup env(true);
    
    std::string handshake = "GET / HTTP/1.1\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Key2: 17  9 G`ZD9   2 2b 7X 3 /r90\r\n\r\n";
    
    env.req.consume(handshake.c_str(),handshake.size());
    env.req.replace_header("Sec-WebSocket-Key3","janelle!");
    
    BOOST_CHECK(websocketpp::processor::is_websocket_handshake(env.req));
    BOOST_CHECK_EQUAL(websocketpp::processor::get_websocket_version(env.req), env.p.get_version());
    BOOST_CHECK_EQUAL( env.p.validate_handshake(env.req), websocketpp::processor::error::missing_required_header );
}

BOOST_AUTO_TEST_CASE( bad_host ) {
	processor_setup env(true);
    websocketpp::uri_ptr u;
    
    std::string handshake = "GET / HTTP/1.1\r\nHost: www.example.com:70000\r\nConnection: upgrade\r\nUpgrade: websocket\r\nOrigin: http://example.com\r\nSec-WebSocket-Key1: 3e6b263  4 17 80\r\nSec-WebSocket-Key2: 17  9 G`ZD9   2 2b 7X 3 /r90\r\n\r\n";
    
    env.req.consume(handshake.c_str(),handshake.size());
    env.req.replace_header("Sec-WebSocket-Key3","janelle!");
    
    BOOST_CHECK(websocketpp::processor::is_websocket_handshake(env.req));
    BOOST_CHECK_EQUAL(websocketpp::processor::get_websocket_version(env.req), env.p.get_version());
    BOOST_CHECK( !env.p.validate_handshake(env.req) );

    BOOST_CHECK_THROW( u = env.p.get_uri(env.req), websocketpp::uri_exception );
}

BOOST_AUTO_TEST_CASE( extract_subprotocols ) {
	processor_setup env(true);
    
    std::vector<std::string> subps;
    
    BOOST_CHECK( !env.p.extract_subprotocols(env.req,subps) );
    BOOST_CHECK_EQUAL( subps.size(), 0 );
}