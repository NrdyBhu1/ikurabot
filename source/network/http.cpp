// http.cpp
// Copyright (c) 2020, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "network.h"

namespace ikura
{
	HttpHeaders::HttpHeaders(std::string_view status)
	{
		this->_status = status;
		this->expected_len = this->_status.size() + 2;
	}

	HttpHeaders& HttpHeaders::add(const std::string& key, const std::string& value)
	{
		this->expected_len += 4 + key.size() + value.size();
		this->_headers.emplace_back(key, value);

		return *this;
	}

	HttpHeaders& HttpHeaders::add(std::string&& key, std::string&& value)
	{
		this->expected_len += 4 + key.size() + value.size();
		this->_headers.emplace_back(std::move(key), std::move(value));

		return *this;
	}

	std::string HttpHeaders::bytes() const
	{
		std::string ret;
		ret.reserve(this->expected_len + 2);

		ret += this->_status;
		ret += "\r\n";

		for(auto& [ k, v] : this->_headers)
			ret += k, ret += ": ", ret += v, ret += "\r\n";

		ret += "\r\n";
		return ret;
	}

	std::string HttpHeaders::status() const
	{
		return this->_status;
	}

	const std::vector<std::pair<std::string, std::string>>& HttpHeaders::headers() const
	{
		return this->_headers;
	}

	std::string HttpHeaders::get(std::string_view key) const
	{
		for(const auto& [ k, v ] : this->_headers)
			if(k == key)
				return v;

		return "";
	}

	std::optional<HttpHeaders> HttpHeaders::parse(const Buffer& buf)
	{
		return parse(std::string_view((const char*) buf.data(), buf.size()));
	}


	std::optional<HttpHeaders> HttpHeaders::parse(std::string_view data)
	{
		auto x = data.find("\r\n");
		if(x == std::string::npos)
			return std::nullopt;

		auto hdrs = HttpHeaders(data.substr(0, x));
		data.remove_prefix(x + 2);

		while(data.find("\r\n") > 0)
		{
			auto ki = data.find(':');
			if(ki == std::string::npos)
				return std::nullopt;

			auto key = std::string(data.substr(0, ki));
			data.remove_prefix(ki + 1);

			// strip spaces
			while(data.size() > 0 && data[0] == ' ')
				data.remove_prefix(1);

			if(data.size() == 0 || data.find("\r\n") == 0)
				return std::nullopt;

			auto vi = data.find("\r\n");
			if(vi == std::string::npos)
				return std::nullopt;

			auto value = std::string(data.substr(0, vi));
			hdrs.add(std::move(key), std::move(value));

			data.remove_prefix(vi + 2);
		}

		if(data.find("\r\n") != 0)
			return std::nullopt;

		return hdrs;
	}
}
