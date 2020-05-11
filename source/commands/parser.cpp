// parser.cpp
// Copyright (c) 2020, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"

#include <tuple>
#include <utility>
#include <functional>

namespace ikura::cmd::ast
{
	using TT = lexer::TokenType;

	template <typename T, typename E = std::string>
	struct Result
	{
		Result() : valid(false) { }

		Result(const T& x) : valid(true), val(x) { }
		Result(T&& x) : valid(true), val(std::move(x)) { }

		Result(const E& e) : valid(false), err(e) { }
		Result(E&& e) : valid(false), err(std::move(e)) { }

		Result(Result&&) = default;
		Result(const Result&) = default;

		Result& operator=(const Result&) = default;
		Result& operator=(Result&&) = default;

		operator bool() const { return this->valid; }
		bool has_value() const { return this->valid; }

		T value() const { assert(this->valid); return this->val; }
		E error() const { assert(!this->valid); return this->err; }

		T unwrap() const { return value(); }

		using result_type = T;
		using error_type = E;

	private:
		bool valid = false;
		T val;
		E err;
	};

	namespace {
		template <typename> struct is_result : std::false_type { };
		template <typename T, typename E> struct is_result<Result<T, E>> : std::true_type { };

		template <typename T> struct remove_array { using type = T; };
		template <typename T, size_t N> struct remove_array<const T(&)[N]> { using type = const T*; };
		template <typename T, size_t N> struct remove_array<T(&)[N]> { using type = T*; };

		// uwu
		template <typename... Ts>
		using concatenator = decltype(std::tuple_cat(std::declval<Ts>()...));

		[[maybe_unused]] std::tuple<> __unwrap() { return { }; }

		template <typename A, typename = std::enable_if_t<is_result<A>::value>>
		std::tuple<std::optional<typename A::result_type>> __unwrap(const A& a)
		{
			return std::make_tuple(a
				? std::optional<typename A::result_type>(a.unwrap())
				: std::nullopt
			);
		}
		template <typename A, typename = std::enable_if_t<std::is_array_v<A>>>
		std::tuple<std::optional<const std::remove_extent_t<A>*>> __unwrap(const A& a) { return std::make_tuple(std::optional(a)); }

		template <typename A, typename = std::enable_if_t<!std::is_array_v<A> && !is_result<A>::value>>
		std::tuple<std::optional<A>> __unwrap(const A& a) { return std::make_tuple(std::optional(a)); }

		template <typename A, typename... As>
		auto __unwrap(const A& a, As&&... as)
		{
			auto x = __unwrap(std::forward<const A&>(a));
			auto xs = std::tuple_cat(__unwrap(as)...);

			return std::tuple_cat(x, xs);
		}

		template <typename A, size_t... Is, typename... As>
		std::tuple<As...> __drop_one_impl(std::tuple<A, As...> tup, std::index_sequence<Is...> seq)
		{
			return std::make_tuple(std::get<(1+Is)>(tup)...);
		}

		template <typename A, typename... As>
		std::tuple<As...> __drop_one(std::tuple<A, As...> tup)
		{
			return __drop_one_impl(tup, std::make_index_sequence<sizeof...(As)>());
		}

		[[maybe_unused]] std::optional<std::tuple<>> __transpose()
		{
			return std::make_tuple();
		}

		template <typename A>
		[[maybe_unused]] std::optional<std::tuple<A>> __transpose(std::tuple<std::optional<A>> tup)
		{
			auto elm = std::get<0>(tup);
			if(!elm.has_value())
				return std::nullopt;

			return elm.value();
		}

		template <typename A, typename... As, typename = std::enable_if_t<sizeof...(As) != 0>>
		[[maybe_unused]] std::optional<std::tuple<A, As...>> __transpose(std::tuple<std::optional<A>, std::optional<As>...> tup)
		{
			auto elm = std::get<0>(tup);
			if(!elm.has_value())
				return std::nullopt;

			auto next = __transpose(__drop_one(tup));
			if(!next.has_value())
				return std::nullopt;

			return std::tuple_cat(std::make_tuple(elm.value()), next.value());
		}



		[[maybe_unused]] std::tuple<> __get_error() { return { }; }

		template <typename A, typename = std::enable_if_t<is_result<A>::value>>
		[[maybe_unused]] std::tuple<typename A::error_type> __get_error(const A& a) { return (a ? typename A::error_type() : a.error()); }

		template <typename A, typename = std::enable_if_t<!is_result<A>::value>>
		[[maybe_unused]] std::tuple<> __get_error(const A& a) { return std::tuple<>(); }


		template <typename A, typename... As>
		auto __get_error(const A& a, As&&... as)
		{
			auto x = __get_error(std::forward<const A&>(a));
			auto xs = std::tuple_cat(__get_error(as)...);

			return std::tuple_cat(x, xs);
		}

		template <typename... Err>
		std::vector<std::string> __concat_errors(Err&&... errs)
		{
			std::vector<std::string> ret;
			(ret.push_back(errs), ...);

			return ret;
		}
	}


	template <typename Ast, typename... Args>
	static Result<Expr*> makeAST(Args&&... args)
	{
		auto opts = __unwrap(std::forward<Args&&>(args)...);
		auto opt = __transpose(opts);
		if(opt.has_value())
		{
			auto foozle = [](auto... xs) -> Expr* {
				return new Ast(xs...);
			};

			return Result<Expr*>(std::apply(foozle, opt.value()));
		}

		auto errs = std::apply([](auto&&... xs) { return __concat_errors(xs...); }, __get_error(std::forward<Args&&>(args)...));
		return util::join(errs, "; ");
	}





	static int get_binary_precedence(TT op)
	{
		switch(op)
		{
			case TT::Period:            return 8000;

			case TT::Exponent:          return 2600;

			case TT::Asterisk:          return 2400;
			case TT::Slash:             return 2200;
			case TT::Percent:           return 2000;

			case TT::Plus:              return 1800;
			case TT::Minus:             return 1800;

			case TT::ShiftLeft:         return 1600;
			case TT::ShiftRight:        return 1600;

			case TT::Ampersand:         return 1400;

			case TT::Caret:             return 1200;

			case TT::Pipe:              return 1000;

			case TT::EqualTo:           return 800;
			case TT::NotEqual:          return 800;
			case TT::LAngle:            return 800;
			case TT::RAngle:            return 800;
			case TT::LessThanEqual:     return 800;
			case TT::GreaterThanEqual:  return 800;

			case TT::LogicalAnd:        return 600;

			case TT::LogicalOr:         return 400;

			case TT::PlusEquals:        return 200;
			case TT::MinusEquals:       return 200;
			case TT::TimesEquals:       return 200;
			case TT::DivideEquals:      return 200;
			case TT::RemainderEquals:   return 200;
			case TT::ShiftLeftEquals:   return 200;
			case TT::ShiftRightEquals:  return 200;
			case TT::BitwiseAndEquals:  return 200;
			case TT::BitwiseOrEquals:   return 200;
			case TT::BitwiseXorEquals:  return 200;
			case TT::ExponentEquals:    return 200;

			case TT::Pipeline:          return 1;

			default:
				return -1;
		}
	}

	struct State
	{
		State(ikura::span<lexer::Token>& ts) : tokens(ts) { }

		bool match(TT t)
		{
			return tokens.size() > 0 && tokens.front() == t;
		}

		const lexer::Token& peek()
		{
			return tokens.empty() ? eof : tokens.front();
		}

		void pop()
		{
			if(!tokens.empty())
				tokens.remove_prefix(1);
		}

		bool empty()
		{
			return tokens.empty();
		}

		lexer::Token eof = lexer::Token(TT::Invalid, "");
		ikura::span<lexer::Token> tokens;
	};


	static Result<Expr*> parseParenthesised(State& st);
	static Result<Expr*> parsePrimary(State& st);
	static Result<Expr*> parseNumber(State& st);
	static Result<Expr*> parseString(State& st);
	static Result<Expr*> parseUnary(State& st);
	static Result<Expr*> parseStmt(State& st);
	static Result<Expr*> parseExpr(State& st);
	static Result<Expr*> parseBool(State& st);

	Expr* parse(ikura::str_view src)
	{
		auto tokens = lexer::lexString(src);
		ikura::span span = tokens;

		auto st = State(span);

		auto result = parseStmt(st);
		if(result)
			return result.unwrap();

		lg::error("cmd", "parse error: %s", result.error());
		return nullptr;
	}




	static Result<Expr*> parseParenthesised(State& st)
	{
		assert(st.peek() == TT::LParen);
		st.pop();

		auto inside = parseExpr(st);

		if(!st.match(TT::RParen))
			return zpr::sprint("expected ')'");

		return inside;
	}

	static Result<Expr*> parsePrimary(State& st)
	{
		switch(st.peek())
		{
			case TT::StringLit:
				return parseString(st);

			case TT::NumberLit:
				return parseNumber(st);

			case TT::BooleanLit:
				return parseBool(st);

			case TT::LParen:
				return parseParenthesised(st);

			default:
				return zpr::sprint("unexpected token '%s'", st.peek().str());
		}
	}

	static Result<Expr*> parseUnary(State& st)
	{
		if(st.match(TT::Exclamation))   return makeAST<UnaryOp>(TT::Exclamation, "!", parseUnary(st));
		else if(st.match(TT::Minus))    return makeAST<UnaryOp>(TT::Minus, "-", parseUnary(st));
		else if(st.match(TT::Plus))     return makeAST<UnaryOp>(TT::Plus, "+", parseUnary(st));
		else                            return parsePrimary(st);
	}

	static Result<Expr*> parseRhs(State& st, Result<Expr*> lhs, int prio)
	{
		if(!lhs)
			return { };

		else if(st.empty() || prio == -1)
			return lhs;

		while(true)
		{
			auto prec = get_binary_precedence(st.peek());
			if(prec < prio)
				return lhs;

			auto oper = st.peek();
			st.pop();

			auto rhs = parseUnary(st);
			if(!rhs) return { };

			auto next = get_binary_precedence(st.peek());
			if(next > prec)
				rhs = parseRhs(st, rhs, prec + 1);

			lhs = makeAST<BinaryOp>(oper.type, oper.str().str(), lhs, rhs);;
		}
	}

	static Result<Expr*> parseExpr(State& st)
	{
		auto lhs = parseUnary(st);
		if(!lhs) return { };

		return parseRhs(st, lhs, 0);
	}


	static Result<Expr*> parseNumber(State& st)
	{
		assert(st.peek() == TT::NumberLit);

		auto num = st.peek().str();
		st.pop();

		auto npos = std::string::npos;
		bool is_floating = (num.find('.') != npos)
						|| (num.find('X') == npos && num.find('x') == npos
							&& (num.find('e') != npos || num.find('E') != npos)
						);

		if(is_floating) return makeAST<LitDouble>(std::stod(num.str()));
		else            return makeAST<LitInteger>(std::stoll(num.str()));
	}

	static Result<Expr*> parseString(State& st)
	{
		return std::string("owo");
	}

	static Result<Expr*> parseBool(State& st)
	{
		assert(st.peek() == TT::NumberLit);
		auto x = st.peek().str();
		st.pop();

		return makeAST<LitBoolean>(x == "true");
	}

	static Result<Expr*> parseStmt(State& st)
	{
		return parseExpr(st);
	}
}