// lexer.cpp
// Copyright (c) 2020, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"

namespace ikura::cmd::lexer
{
	using TT = TokenType;

	static Token INVALID = Token(TT::Invalid, "");
	static bool is_digit(char c) { return '0' <= c && c <= '9'; }
	static bool is_letter(char c) { return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'A'); }

	static ikura::string_map<TT> keywordMap;
	static void initKeywordMap()
	{
		if(!keywordMap.empty())
			return;

		keywordMap["fn"]     = TT::Function;
		keywordMap["if"]     = TT::If;
		keywordMap["let"]    = TT::Let;
		keywordMap["else"]   = TT::Else;
		keywordMap["while"]  = TT::While;
		keywordMap["return"] = TT::Return;
		keywordMap["for"]    = TT::For;
		keywordMap["true"]   = TT::BooleanLit;
		keywordMap["false"]  = TT::BooleanLit;
	}




	static Token lex_one_token(ikura::str_view& src, TT prevType)
	{
		// skip all whitespace.
		while(src.size() > 0 && (src[0] == ' ' || src[0] == '\t' || src[0] == '\n' || src[0] == '\r'))
			src.remove_prefix(1);

		if(src.empty())
			return Token(TT::EndOfFile, "");

		if(src.find("**=") == 0)
		{
			auto ret = Token(TT::ExponentEquals, src.take(3));
			src.remove_prefix(3);
			return ret;
		}
		else if(src.find("<<=") == 0)
		{
			auto ret = Token(TT::ShiftLeftEquals, src.take(3));
			src.remove_prefix(3);
			return ret;
		}
		else if(src.find(">>=") == 0)
		{
			auto ret = Token(TT::ShiftRightEquals, src.take(3));
			src.remove_prefix(3);
			return ret;
		}
		else if(src.find("&&") == 0)
		{
			auto ret = Token(TT::LogicalAnd, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("||") == 0)
		{
			auto ret = Token(TT::LogicalOr, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("==") == 0)
		{
			auto ret = Token(TT::EqualTo, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("!=") == 0)
		{
			auto ret = Token(TT::NotEqual, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("<=") == 0)
		{
			auto ret = Token(TT::LessThanEqual, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find(">=") == 0)
		{
			auto ret = Token(TT::GreaterThanEqual, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("<<") == 0)
		{
			auto ret = Token(TT::ShiftLeft, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find(">>") == 0)
		{
			auto ret = Token(TT::ShiftRight, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("**") == 0)
		{
			auto ret = Token(TT::Exponent, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("|>") == 0)
		{
			auto ret = Token(TT::Pipeline, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("+=") == 0)
		{
			auto ret = Token(TT::PlusEquals, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("-=") == 0)
		{
			auto ret = Token(TT::MinusEquals, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("*=") == 0)
		{
			auto ret = Token(TT::TimesEquals, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("/=") == 0)
		{
			auto ret = Token(TT::DivideEquals, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("%=") == 0)
		{
			auto ret = Token(TT::RemainderEquals, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("^=") == 0)
		{
			auto ret = Token(TT::BitwiseXorEquals, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("&=") == 0)
		{
			auto ret = Token(TT::BitwiseAndEquals, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("|=") == 0)
		{
			auto ret = Token(TT::BitwiseOrEquals, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if(src.find("->") == 0)
		{
			auto ret = Token(TT::RightArrow, src.take(2));
			src.remove_prefix(2);
			return ret;
		}
		else if('0' <= src[0] && src[0] <= '9')
		{
			auto tmp = src;

			int base = 10;

			if(tmp.find("0x") == 0 || tmp.find("0X") == 0)
				base = 16, tmp.remove_prefix(2);

			else if(tmp.find("0b") == 0 || tmp.find("0B") == 0)
				base = 2, tmp.remove_prefix(2);

			// find that shit
			auto end = std::find_if_not(tmp.begin(), tmp.end(), [base](const char& c) -> bool {
				if(base == 10)	return is_digit(c);
				if(base == 16)	return is_digit(c) || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
				else			return (c == '0' || c == '1');
			});

			tmp.remove_prefix((end - tmp.begin()));

			// check if we have 'e' or 'E'
			bool hadExp = false;
			if(tmp.size() > 0 && (tmp[0] == 'e' || tmp[0] == 'E'))
			{
				if(base != 10)
				{
					lg::error("cmd/lex", "exponential form is supported with neither hexadecimal nor binary literals");
					return INVALID;
				}

				// find that shit
				auto next = std::find_if_not(tmp.begin() + 1, tmp.end(), is_digit);

				// this does the 'e' as well.
				tmp.remove_prefix(next - tmp.begin());

				hadExp = true;
			}

			size_t didRead = src.size() - tmp.size();
			auto post = src.substr(didRead);

			if(!post.empty() && post[0] == '.')
			{
				if(base != 10)
				{
					lg::error("cmd/lex", "invalid floating point literal; only valid in base 10");
					return INVALID;
				}
				else if(hadExp)
				{
					lg::error("cmd/lex", "invalid floating point literal; decimal point cannot occur after the exponent ('e' or 'E').");
					return INVALID;
				}

				// if the previous token was a '.' as well, then we're doing some tuple access
				// eg. x.0.1 (we would be at '0', having a period both ahead and behind us)

				// if the next token is not a number, then same thing, eg.
				// x.0.z, where the first tuple element of 'x' is a struct or something.

				// so -- lex a floating point *iff* the previous token was not '.', and the next token is a digit.
				if(prevType != TT::Period && post.size() > 1 && is_digit(post[1]))
				{
					// yes, parse a floating point
					post.remove_prefix(1), didRead++;

					while(post.size() > 0 && is_digit(post.front()))
						post.remove_prefix(1), didRead++;

					// ok.
				}
				else
				{
					// no, just return the integer token.
					// (which we do below, so just do nothing here)
				}
			}

			auto ret = Token(TT::NumberLit, src.substr(0, didRead));
			src.remove_prefix(didRead);
			return ret;
		}
		else if(src[0] == '"')
		{
			if(src.size() == 1)
			{
				lg::error("cmd/lex", "unexpected end of input");
				return INVALID;
			}

			auto tmp = src;

			tmp.remove_prefix(1);
			size_t count = 0;

			// don't handle escapes here, because we must strictly return a string_view.
			while(tmp[0] != '"')
			{
				size_t skip = 0;

				if(tmp.size() > 1 && tmp[0] == '\\' && tmp[1] == '"')
					skip = 2;

				else if(tmp[0] == '"')
					break;

				else
					skip = 1;

				count += skip;
				tmp.remove_prefix(skip);
			}

			// tmp[0] == '"'
			tmp.remove_prefix(1);

			auto ret = Token(TT::StringLit, src.drop(1).take(count));
			src = tmp;

			return ret;
		}
		else if(src[0] == '_' || is_letter(src[0]))
		{
			// TODO: handle unicode.
			size_t identLength = 1;
			auto tmp = src.drop(1);

			while(is_digit(tmp[0]) || is_letter(tmp[0]) || tmp[0] == '_')
				tmp.remove_prefix(1), identLength++;

			size_t read = identLength;
			auto text = src.substr(0, identLength);

			auto type = TT::Invalid;
			initKeywordMap();
			if(auto it = keywordMap.find(text); it != keywordMap.end())
				type = it->second;

			else
				type = TT::Identifier;

			auto ret = Token(type, text);
			src.remove_prefix(read);
			return ret;
		}
		else
		{
			Token ret;
			switch(src[0])
			{
				case ';': ret = Token(TT::Semicolon, src.take(1));      break;
				case '$': ret = Token(TT::Dollar, src.take(1));         break;
				case ':': ret = Token(TT::Colon, src.take(1));          break;
				case '|': ret = Token(TT::Pipe, src.take(1));           break;
				case '&': ret = Token(TT::Ampersand, src.take(1));      break;
				case '.': ret = Token(TT::Period, src.take(1));         break;
				case '*': ret = Token(TT::Asterisk, src.take(1));       break;
				case '^': ret = Token(TT::Caret, src.take(1));          break;
				case '!': ret = Token(TT::Exclamation, src.take(1));    break;
				case '+': ret = Token(TT::Plus, src.take(1));           break;
				case ',': ret = Token(TT::Comma, src.take(1));          break;
				case '-': ret = Token(TT::Minus, src.take(1));          break;
				case '/': ret = Token(TT::Slash, src.take(1));          break;
				case '(': ret = Token(TT::LParen, src.take(1));         break;
				case ')': ret = Token(TT::RParen, src.take(1));         break;
				case '[': ret = Token(TT::LSquare, src.take(1));        break;
				case ']': ret = Token(TT::RSquare, src.take(1));        break;
				case '{': ret = Token(TT::LBrace, src.take(1));         break;
				case '}': ret = Token(TT::RBrace, src.take(1));         break;
				case '<': ret = Token(TT::LAngle, src.take(1));         break;
				case '>': ret = Token(TT::RAngle, src.take(1));         break;
				case '=': ret = Token(TT::Equal, src.take(1));          break;
				case '%': ret = Token(TT::Percent, src.take(1));        break;
				default:  ret = Token(TT::Invalid, src.take(1));        break;
			}

			src.remove_prefix(1);
			return ret;
		}
	}


	std::vector<Token> lexString(ikura::str_view src)
	{
		std::vector<Token> ret;
		Token tok;

		while((tok = lex_one_token(src, tok.type)) != TT::EndOfFile)
			ret.push_back(std::move(tok));

		return ret;
	}
}


