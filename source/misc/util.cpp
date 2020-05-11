// util.cpp
// Copyright (c) 2020, zhiayang
// Licensed under the Apache License Version 2.0.

#include <limits>
#include <random>
#include <chrono>
#include <fstream>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "defs.h"

namespace ikura
{
	namespace util
	{
		std::string join(const ikura::span<ikura::str_view>& xs, char delim)
		{
			std::string ret;
			for(size_t i = 0; i < xs.size(); i++)
			{
				ret += xs[i];
				if(i + 1 != xs.size())
					ret += delim;
			}

			return ret;
		}


		std::vector<ikura::str_view> split(ikura::str_view view, char delim)
		{
			std::vector<ikura::str_view> ret;

			while(true)
			{
				size_t ln = view.find(delim);

				if(ln != ikura::str_view::npos)
				{
					ret.emplace_back(view.data(), ln);
					view.remove_prefix(ln + 1);
				}
				else
				{
					break;
				}
			}

			// account for the case when there's no trailing newline, and we still have some stuff stuck in the view.
			if(!view.empty())
				ret.emplace_back(view.data(), view.length());

			return ret;
		};


		uint64_t getMillisecondTimestamp()
		{
			return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		}


		size_t getFileSize(const std::string& path)
		{
			struct stat st;
			if(stat(path.c_str(), &st) != 0)
			{
				char buf[128] = { 0 };
				strerror_r(errno, buf, 127);
				lg::error("misc", "failed to get filesize for '%s' (error code %d / %s)", path, errno, buf);

				return -1;
			}

			return st.st_size;
		}

		std::pair<uint8_t*, size_t> readEntireFile(const std::string& path)
		{
			auto bad = std::pair(nullptr, 0);;

			auto sz = getFileSize(path);
			if(sz == static_cast<size_t>(-1)) return bad;

			// i'm lazy, so just use fstreams.
			auto fs = std::fstream(path);
			if(!fs.good()) return bad;


			uint8_t* buf = new uint8_t[sz + 1];
			fs.read(reinterpret_cast<char*>(buf), sz);
			fs.close();

			return std::pair(buf, sz);
		}

		std::tuple<int, uint8_t*, size_t> mmapEntireFile(const std::string& path)
		{
			auto bad = std::tuple(-1, nullptr, 0);;

			auto sz = getFileSize(path);
			if(sz == static_cast<size_t>(-1))
				return bad;

			auto fd = open(path.c_str(), O_RDONLY, 0);
			if(fd == -1)
				return bad;

			auto buf = (uint8_t*) mmap(0, sz, PROT_READ, MAP_PRIVATE, fd, 0);
			if(buf == (void*) (-1))
			{
				perror("there was an error reading the file");
				exit(-1);
			}

			return { fd, buf, sz };
		}

		void munmapEntireFile(int fd, uint8_t* buf, size_t len)
		{
			munmap((void*) buf, len);
			close(fd);
		}
	}

	namespace random
	{
		// this is kinda dumb but... meh.
		template <typename T>
		struct rd_state_t
		{
			rd_state_t() : mersenne(std::random_device()()),
				distribution(std::numeric_limits<T>::min(), std::numeric_limits<T>::max()) { }

			std::mt19937 mersenne;
			std::uniform_int_distribution<T> distribution;
		};

		template <typename T>
		rd_state_t<T> rd_state;


		template <typename T>
		T get()
		{
			auto& st = rd_state<T>;
			return st.distribution(st.mersenne);
		}

		template uint8_t  get<uint8_t>();
		template uint16_t get<uint16_t>();
		template uint32_t get<uint32_t>();
		template uint64_t get<uint64_t>();
	}

	namespace value
	{
		constexpr bool IS_BIG = (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__);

		template <> uint16_t to_native(uint16_t x) { if constexpr (IS_BIG) return __builtin_bswap16(x); else return x; }
		template <> uint32_t to_native(uint32_t x) { if constexpr (IS_BIG) return __builtin_bswap32(x); else return x; }
		template <> uint64_t to_native(uint64_t x) { if constexpr (IS_BIG) return __builtin_bswap64(x); else return x; }

		template <> uint16_t to_network(uint16_t x) { if constexpr (IS_BIG) return __builtin_bswap16(x); else return x; }
		template <> uint32_t to_network(uint32_t x) { if constexpr (IS_BIG) return __builtin_bswap32(x); else return x; }
		template <> uint64_t to_network(uint64_t x) { if constexpr (IS_BIG) return __builtin_bswap64(x); else return x; }


		template <> int16_t to_native(int16_t x) { if constexpr (IS_BIG) return __builtin_bswap16(x); else return x; }
		template <> int32_t to_native(int32_t x) { if constexpr (IS_BIG) return __builtin_bswap32(x); else return x; }
		template <> int64_t to_native(int64_t x) { if constexpr (IS_BIG) return __builtin_bswap64(x); else return x; }

		template <> int16_t to_network(int16_t x) { if constexpr (IS_BIG) return __builtin_bswap16(x); else return x; }
		template <> int32_t to_network(int32_t x) { if constexpr (IS_BIG) return __builtin_bswap32(x); else return x; }
		template <> int64_t to_network(int64_t x) { if constexpr (IS_BIG) return __builtin_bswap64(x); else return x; }
	}
}
