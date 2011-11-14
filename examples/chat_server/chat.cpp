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

#include "chat.hpp"

#include <boost/algorithm/string/replace.hpp>

using websocketchat::chat_server_handler;
using websocketpp::session::server_session_ptr;


void chat_server_handler::validate(server_session_ptr session) {
	std::stringstream err;
	
	// We only know about the chat resource
	if (session->get_resource() != "/chat") {
		err << "Request for unknown resource " << session->get_resource();
		throw(websocketpp::http::exception(err.str(),websocketpp::http::status_code::NOT_FOUND));
	}
	
	// Require specific origin example
	if (session->get_origin() != "http://zaphoyd.com") {
		err << "Request from unrecognized origin: " << session->get_origin();
		throw(websocketpp::http::exception(err.str(),websocketpp::http::status_code::FORBIDDEN));
	}
}


void chat_server_handler::on_open(server_session_ptr session) {
	std::cout << "client " << session << " joined the lobby." << std::endl;
	m_connections.insert(std::pair<server_session_ptr,std::string>(session,get_con_id(session)));

	// send user list and signon message to all clients
	send_to_all(serialize_state());
	session->send(encode_message("server","Welcome, use the /alias command to set a name, /help for a list of other commands."));
	send_to_all(encode_message("server",m_connections[session]+" has joined the chat."));
}

void chat_server_handler::on_close(server_session_ptr session) {
	std::map<server_session_ptr,std::string>::iterator it = m_connections.find(session);
	
	if (it == m_connections.end()) {
		// this client has already disconnected, we can ignore this.
		// this happens during certain types of disconnect where there is a
		// deliberate "soft" disconnection preceeding the "hard" socket read
		// fail or disconnect ack message.
		return;
	}
	
	std::cout << "client " << session << " left the lobby." << std::endl;
	
	const std::string alias = it->second;
	m_connections.erase(it);

	// send user list and signoff message to all clients
	send_to_all(serialize_state());
	send_to_all(encode_message("server",alias+" has left the chat."));
}

void chat_server_handler::on_message(server_session_ptr session,websocketpp::utf8_string_ptr msg) {
	std::cout << "message from client " << session << ": " << *msg << std::endl;
	
	
	
	// check for special command messages
	if (*msg == "/help") {
		// print command list
		session->send(encode_message("server","avaliable commands:<br />&nbsp;&nbsp;&nbsp;&nbsp;/help - show this help<br />&nbsp;&nbsp;&nbsp;&nbsp;/alias foo - set alias to foo",false));
		return;
	}
	
	if (msg->substr(0,7) == "/alias ") {
		std::string response;
		std::string alias;
		
		if (msg->size() == 7) {
			response = "You must enter an alias.";
			session->send(encode_message("server",response));
			return;
		} else {
			alias = msg->substr(7);
		}
		
		response = m_connections[session] + " is now known as "+alias;

		// store alias pre-escaped so we don't have to do this replacing every time this
		// user sends a message
		
		// escape JSON characters
		boost::algorithm::replace_all(alias,"\\","\\\\");
		boost::algorithm::replace_all(alias,"\"","\\\"");
		
		// escape HTML characters
		boost::algorithm::replace_all(alias,"&","&amp;");
		boost::algorithm::replace_all(alias,"<","&lt;");
		boost::algorithm::replace_all(alias,">","&gt;");
		
		m_connections[session] = alias;
		
		// set alias
		send_to_all(serialize_state());
		send_to_all(encode_message("server",response));
		return;
	}
	
	// catch other slash commands
	if ((*msg)[0] == '/') {
		session->send(encode_message("server","unrecognized command"));
		return;
	}
	
	// create JSON message to send based on msg
	send_to_all(encode_message(m_connections[session],*msg));
}

// {"type":"participants","value":[<participant>,...]}
std::string chat_server_handler::serialize_state() {
	std::stringstream s;
	
	s << "{\"type\":\"participants\",\"value\":[";
	
	std::map<server_session_ptr,std::string>::iterator it;
	
	for (it = m_connections.begin(); it != m_connections.end(); it++) {
		s << "\"" << (*it).second << "\"";
		if (++it != m_connections.end()) {
			s << ",";
		}
		it--;
	}
	
	s << "]}";
	
	return s.str();
}

// {"type":"msg","sender":"<sender>","value":"<msg>" }
std::string chat_server_handler::encode_message(std::string sender,std::string msg,bool escape) {
	std::stringstream s;
	
	// escape JSON characters
	boost::algorithm::replace_all(msg,"\\","\\\\");
	boost::algorithm::replace_all(msg,"\"","\\\"");
	
	// escape HTML characters
	if (escape) {
		boost::algorithm::replace_all(msg,"&","&amp;");
		boost::algorithm::replace_all(msg,"<","&lt;");
		boost::algorithm::replace_all(msg,">","&gt;");
	}
	
	s << "{\"type\":\"msg\",\"sender\":\"" << sender 
	  << "\",\"value\":\"" << msg << "\"}";
	
	return s.str();
}

std::string chat_server_handler::get_con_id(server_session_ptr s) {
	std::stringstream endpoint;
	endpoint << s->get_endpoint();
	return endpoint.str();
}

void chat_server_handler::send_to_all(std::string data) {
	std::map<server_session_ptr,std::string>::iterator it;
	for (it = m_connections.begin(); it != m_connections.end(); it++) {
		(*it).first->send(data);
	}
}