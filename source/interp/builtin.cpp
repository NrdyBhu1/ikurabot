// builtin.cpp
// Copyright (c) 2020, zhiayang
// Licensed under the Apache License Version 2.0.

#include "zfu.h"
#include "ast.h"
#include "cmd.h"
#include "timer.h"

namespace ikura::cmd
{
	// defined in command.cpp
	Message value_to_message(const interp::Value& val);
}

namespace ikura::interp
{
	static void command_def(CmdContext& cs, const Channel* chan, ikura::str_view arg_str);
	static void command_eval(CmdContext& cs, const Channel* chan, ikura::str_view arg_str);
	static void command_show(CmdContext& cs, const Channel* chan, ikura::str_view arg_str);
	static void command_redef(CmdContext& cs, const Channel* chan, ikura::str_view arg_str);
	static void command_undef(CmdContext& cs, const Channel* chan, ikura::str_view arg_str);
	static void command_chmod(CmdContext& cs, const Channel* chan, ikura::str_view arg_str);
	static void command_global(CmdContext& cs, const Channel* chan, ikura::str_view arg_str);

	bool is_builtin_command(ikura::str_view x)
	{
		return zfu::match(x, "def", "eval", "show", "redef", "undef", "chmod", "global");
	}

	// tsl::robin_map doesn't let us do this for some reason, so just fall back to std::unordered_map.
	static std::unordered_map<std::string, void (*)(CmdContext&, const Channel*, ikura::str_view)> builtin_cmds = {
		{ "chmod",  command_chmod  },
		{ "eval",   command_eval   },
		{ "global", command_global },
		{ "def",    command_def    },
		{ "redef",  command_redef  },
		{ "undef",  command_undef  },
		{ "show",   command_show   },
	};

	bool run_builtin_command(CmdContext& cs, const Channel* chan, ikura::str_view cmd_str, ikura::str_view arg_str)
	{
		auto user_perms = chan->getUserPermissions(cs.caller);
		auto denied = [&]() -> bool {
			lg::warn("cmd", "user '%s' tried to execute command '%s' with insufficient permissions (%x)",
				cs.caller, cmd_str, user_perms);

			chan->sendMessage(Message("insufficient permissions"));
			return true;
		};

		uint32_t perm = interpreter().map_read([&](auto& interp) -> uint32_t {
			if(auto it = interp.builtinCommandPermissions.find(cmd_str); it != interp.builtinCommandPermissions.end())
				return it->second;

			return 0;
		});

		if(!cmd::verifyPermissions(perm, user_perms))
			return denied();

		if(is_builtin_command(cmd_str))
		{
			builtin_cmds[cmd_str.str()](cs, chan, arg_str);
			return true;
		}

		return false;
	}







	static void command_eval(CmdContext& cs, const Channel* chan, ikura::str_view arg_str)
	{
		// syntax: eval <expr>
		auto t = ikura::timer();

		auto ret = interpreter().wlock()->evaluateExpr(arg_str, cs);
		lg::log("interp", "command took %.3f ms to execute", t.measure());

		if(ret) chan->sendMessage(cmd::value_to_message(ret.value()));
	}

	static void command_chmod(CmdContext& cs, const Channel* chan, ikura::str_view arg_str)
	{
		// syntax: chmod <command> <permissions>
		auto cmd = arg_str.substr(0, arg_str.find(' ')).trim();
		auto perm_str = arg_str.drop(cmd.size()).trim();

		if(cmd.empty() || perm_str.empty())
			return chan->sendMessage(Message("not enough arguments to chmod"));
		auto tmp = perm_str.str();
		char* tmp2 = nullptr;

		auto perm = strtol(tmp.data(), &tmp2, /* base: */ 0x10);
		if(tmp2 != &tmp.back() + 1)
			return chan->sendMessage(Message(zpr::sprint("invalid permission string '%s'", tmp)));

		if(is_builtin_command(cmd))
		{
			interpreter().wlock()->builtinCommandPermissions[cmd] = perm;
		}
		else
		{
			auto command = interpreter().rlock()->findCommand(cmd);
			if(!command)
				return chan->sendMessage(Message(zpr::sprint("'%s' does not exist", cmd)));

			command->setPermissions(perm);
		}

		chan->sendMessage(Message(zpr::sprint("permissions for '%s' changed to %x", cmd, perm)));
	}

	static void command_global(CmdContext& cs, const Channel* chan, ikura::str_view arg_str)
	{
		// syntax: global <name> <type>
		auto name = arg_str.substr(0, arg_str.find(' ')).trim();
		auto type_str = arg_str.drop(name.size()).trim();

		if(name.empty() || type_str.empty())
			return chan->sendMessage(Message("not enough arguments to global"));

		auto value = ast::parseType(type_str);
		if(!value)
			return chan->sendMessage(Message(zpr::sprint("invalid type '%s'", type_str)));

		interpreter().wlock()->addGlobal(name, value.value());
		chan->sendMessage(Message(zpr::sprint("added global '%s' with type '%s'", name, value->type()->str())));
	}

	static void internal_def(const Channel* chan, bool redef, ikura::str_view name, ikura::str_view expansion)
	{
		if(interpreter().rlock()->findCommand(name) != nullptr)
		{
			if(!redef)  return chan->sendMessage(Message(zpr::sprint("'%s' is already defined", name)));
			else        interpreter().wlock()->removeCommandOrAlias(name);
		}
		else if(redef)
		{
			return chan->sendMessage(Message(zpr::sprint("'%s' does not exist", name)));
		}

		interpreter().wlock()->commands.emplace(name, new Macro(name.str(), expansion));
		chan->sendMessage(Message(zpr::sprint("%sdefined '%s'", redef ? "re" : "", name)));
	}

	static void command_def(CmdContext& cs, const Channel* chan, ikura::str_view arg_str)
	{
		// syntax: def <name> expansion...
		auto name = arg_str.substr(0, arg_str.find(' ')).trim();
		auto expansion = arg_str.drop(name.size()).trim();

		if(name.empty())        return chan->sendMessage(Message("not enough arguments to 'def'"));
		if(expansion.empty())   return chan->sendMessage(Message("'def' expansion cannot be empty"));

		internal_def(chan, false, name, expansion);
	}

	static void command_redef(CmdContext& cs, const Channel* chan, ikura::str_view arg_str)
	{
		// syntax: def <name> expansion...
		auto name = arg_str.substr(0, arg_str.find(' ')).trim();
		auto expansion = arg_str.drop(name.size()).trim();

		if(name.empty())        return chan->sendMessage(Message("not enough arguments to 'redef'"));
		if(expansion.empty())   return chan->sendMessage(Message("'redef' expansion cannot be empty"));

		internal_def(chan, true, name, expansion);
	}

	static void command_undef(CmdContext& cs, const Channel* chan, ikura::str_view arg_str)
	{
		// syntax: undef <name>
		if(arg_str.find(' ') != std::string::npos || arg_str.empty())
			return chan->sendMessage(Message("'undef' takes exactly 1 argument"));

		auto done = interpreter().wlock()->removeCommandOrAlias(arg_str);

		chan->sendMessage(Message(
			done ? zpr::sprint("removed '%s'", arg_str)
				 : zpr::sprint("'%s' does not exist", arg_str)
		));
	}

	static void command_show(CmdContext& cs, const Channel* chan, ikura::str_view arg_str)
	{
		// syntax: show <name>
		if(arg_str.find(' ') != std::string::npos || arg_str.empty())
			return chan->sendMessage(Message("'show' takes exactly 1 argument"));

		if(is_builtin_command(arg_str))
			return chan->sendMessage(Message(zpr::sprint("'%s' is a builtin command", arg_str)));

		auto cmd = interpreter().rlock()->findCommand(arg_str);
		if(cmd == nullptr)
			return chan->sendMessage(Message(zpr::sprint("'%s' does not exist", arg_str)));

		if(auto macro = dynamic_cast<Macro*>(cmd))
		{
			Message msg;
			msg.add(zpr::sprint("'%s' is defined as: ", arg_str));

			auto code = macro->getCode();
			for(const auto& c : code)
				msg.add(c);

			return chan->sendMessage(msg);
		}
		else
		{
			return chan->sendMessage(Message(zpr::sprint("'%s' cannot be shown", arg_str)).add(Emote("monkaTOS")));
		}
	}
}




namespace ikura::interp
{
	static constexpr auto t_fn = Type::get_function;
	static constexpr auto t_int = Type::get_integer;
	static constexpr auto t_str = Type::get_string;
	static constexpr auto t_dbl = Type::get_double;
	static constexpr auto t_map = Type::get_map;
	static constexpr auto t_char = Type::get_char;
	static constexpr auto t_bool = Type::get_bool;
	static constexpr auto t_void = Type::get_void;
	static constexpr auto t_list = Type::get_list;

	static std::optional<interp::Value> fn_int_to_int(InterpState* fs, CmdContext& cs);
	static std::optional<interp::Value> fn_str_to_int(InterpState* fs, CmdContext& cs);
	static std::optional<interp::Value> fn_dbl_to_int(InterpState* fs, CmdContext& cs);
	static std::optional<interp::Value> fn_char_to_int(InterpState* fs, CmdContext& cs);
	static std::optional<interp::Value> fn_bool_to_int(InterpState* fs, CmdContext& cs);

	static auto bfn_int_to_int  = BuiltinFunction("int", t_fn(t_int(), { t_int() }), &fn_int_to_int);
	static auto bfn_str_to_int  = BuiltinFunction("int", t_fn(t_int(), { t_str() }), &fn_str_to_int);
	static auto bfn_dbl_to_int  = BuiltinFunction("int", t_fn(t_int(), { t_dbl() }), &fn_dbl_to_int);
	static auto bfn_char_to_int = BuiltinFunction("int", t_fn(t_int(), { t_char() }), &fn_char_to_int);
	static auto bfn_bool_to_int = BuiltinFunction("int", t_fn(t_int(), { t_bool() }), &fn_bool_to_int);

	static std::optional<interp::Value> fn_str_to_str(InterpState* fs, CmdContext& cs);
	static std::optional<interp::Value> fn_int_to_str(InterpState* fs, CmdContext& cs);
	static std::optional<interp::Value> fn_dbl_to_str(InterpState* fs, CmdContext& cs);
	static std::optional<interp::Value> fn_map_to_str(InterpState* fs, CmdContext& cs);
	static std::optional<interp::Value> fn_list_to_str(InterpState* fs, CmdContext& cs);
	static std::optional<interp::Value> fn_char_to_str(InterpState* fs, CmdContext& cs);
	static std::optional<interp::Value> fn_bool_to_str(InterpState* fs, CmdContext& cs);

	static auto bfn_str_to_str  = BuiltinFunction("str", t_fn(t_str(), { t_str() }), &fn_str_to_str);
	static auto bfn_int_to_str  = BuiltinFunction("str", t_fn(t_str(), { t_int() }), &fn_int_to_str);
	static auto bfn_dbl_to_str  = BuiltinFunction("str", t_fn(t_str(), { t_dbl() }), &fn_dbl_to_str);
	static auto bfn_bool_to_str = BuiltinFunction("str", t_fn(t_str(), { t_bool() }), &fn_bool_to_str);
	static auto bfn_char_to_str = BuiltinFunction("str", t_fn(t_str(), { t_char() }), &fn_char_to_str);
	static auto bfn_list_to_str = BuiltinFunction("str", t_fn(t_str(), { t_list(t_void()) }), &fn_list_to_str);
	static auto bfn_map_to_str  = BuiltinFunction("str", t_fn(t_str(), { t_map(t_void(), t_void()) }), &fn_map_to_str);

	static std::unordered_map<std::string, FunctionOverloadSet> builtin_fns = {
		{
			"int", FunctionOverloadSet("int", {
				&bfn_int_to_int, &bfn_str_to_int, &bfn_dbl_to_int, &bfn_bool_to_int, &bfn_char_to_int,
			})
		},

		{
			"str", FunctionOverloadSet("str", {
				&bfn_str_to_str, &bfn_int_to_str, &bfn_dbl_to_str, &bfn_bool_to_str, &bfn_char_to_str,
				&bfn_list_to_str, &bfn_map_to_str,
			})
		}
	};


	Command* getBuiltinFunction(ikura::str_view name)
	{
		if(auto it = builtin_fns.find(name.str()); it != builtin_fns.end())
			return &it->second;

		return nullptr;
	}




	std::optional<interp::Value> BuiltinFunction::run(InterpState* fs, CmdContext& cs) const
	{
		return this->action(fs, cs);
	}

	std::optional<interp::Value> FunctionOverloadSet::run(InterpState* fs, CmdContext& cs) const
	{
		int score = INT_MAX;
		Command* best = 0;

		std::vector<Type::Ptr> arg_types;
		for(const auto& a : cs.macro_args)
			arg_types.push_back(a.type());

		for(auto cand : this->functions)
		{
			auto cand_args = cand->getSignature()->arg_types();

			if(arg_types.size() != cand_args.size())
				continue;

			int cost = 0;
			for(size_t i = 0; i < arg_types.size(); i++)
			{
				int k = arg_types[i]->get_cast_dist(cand_args[i]);
				if(k == -1) goto fail;
				else        cost += k;
			}

			if(cost < score)
			{
				score = cost;
				best = cand;
			}
		fail:
			;
		}

		if(!best)
		{
			lg::error("interp", "no matching function for call to '%s'", this->name);
			return { };
		}

		return best->run(fs, cs);
	}






	static std::optional<interp::Value> fn_int_to_int(InterpState* fs, CmdContext& cs)
	{
		if(cs.macro_args.empty() || !cs.macro_args[0].type()->is_integer())
			return { };

		return cs.macro_args[0];
	}

	static std::optional<interp::Value> fn_str_to_int(InterpState* fs, CmdContext& cs)
	{
		if(cs.macro_args.empty() || !cs.macro_args[0].type()->is_string())
			return { };

		auto s = cs.macro_args[0].raw_str();

		char* end = nullptr;
		auto ret = strtoll(s.c_str(), &end, 10);
		if(end != &s.back() + 1)
			return { };

		return Value::of_integer(ret);
	}

	static std::optional<interp::Value> fn_dbl_to_int(InterpState* fs, CmdContext& cs)
	{
		if(cs.macro_args.empty() || !cs.macro_args[0].type()->is_double())
			return { };

		return Value::of_integer((int64_t) cs.macro_args[0].get_double());
	}

	static std::optional<interp::Value> fn_char_to_int(InterpState* fs, CmdContext& cs)
	{
		if(cs.macro_args.empty() || !cs.macro_args[0].type()->is_char())
			return { };

		return Value::of_integer((int64_t) cs.macro_args[0].get_char());
	}

	static std::optional<interp::Value> fn_bool_to_int(InterpState* fs, CmdContext& cs)
	{
		if(cs.macro_args.empty() || !cs.macro_args[0].type()->is_bool())
			return { };

		return Value::of_integer(cs.macro_args[0].get_bool() ? 1 : 0);
	}




	static std::optional<interp::Value> fn_str_to_str(InterpState* fs, CmdContext& cs)
	{
		if(cs.macro_args.empty() || !cs.macro_args[0].type()->is_string())
			return { };

		return cs.macro_args[0];
	}

	static std::optional<interp::Value> fn_int_to_str(InterpState* fs, CmdContext& cs)
	{

		if(cs.macro_args.empty() || !cs.macro_args[0].type()->is_integer())
			return { };

		return Value::of_string(cs.macro_args[0].str());
	}

	static std::optional<interp::Value> fn_dbl_to_str(InterpState* fs, CmdContext& cs)
	{

		if(cs.macro_args.empty() || !cs.macro_args[0].type()->is_double())
			return { };

		return Value::of_string(cs.macro_args[0].str());
	}

	static std::optional<interp::Value> fn_map_to_str(InterpState* fs, CmdContext& cs)
	{

		if(cs.macro_args.empty() || !cs.macro_args[0].type()->is_map())
			return { };

		return Value::of_string(cs.macro_args[0].str());
	}

	static std::optional<interp::Value> fn_list_to_str(InterpState* fs, CmdContext& cs)
	{

		if(cs.macro_args.empty() || !cs.macro_args[0].type()->is_list())
			return { };

		return Value::of_string(cs.macro_args[0].str());
	}

	static std::optional<interp::Value> fn_char_to_str(InterpState* fs, CmdContext& cs)
	{

		if(cs.macro_args.empty() || !cs.macro_args[0].type()->is_char())
			return { };

		return Value::of_string(cs.macro_args[0].str());
	}

	static std::optional<interp::Value> fn_bool_to_str(InterpState* fs, CmdContext& cs)
	{

		if(cs.macro_args.empty() || !cs.macro_args[0].type()->is_bool())
			return { };

		return Value::of_string(cs.macro_args[0].str());
	}





























	FunctionOverloadSet::FunctionOverloadSet(std::string name, std::vector<Command*> fns)
		: Command(name, Type::get_macro_function()), functions(std::move(fns)) { }

	void FunctionOverloadSet::serialise(Buffer& buf) const { assert(!"not supported"); }
	void FunctionOverloadSet::deserialise(Span& buf) { assert(!"not supported"); }


	BuiltinFunction::BuiltinFunction(std::string name, Type::Ptr type,
		std::optional<interp::Value> (*action)(InterpState*, CmdContext&)) : Command(std::move(name), std::move(type)), action(action) { }

	void BuiltinFunction::serialise(Buffer& buf) const { assert(!"not supported"); }
	void BuiltinFunction::deserialise(Span& buf) { assert(!"not supported"); }
}