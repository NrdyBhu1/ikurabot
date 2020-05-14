// cmd.h
// Copyright (c) 2020, zhiayang
// Licensed under the Apache License Version 2.0.

#include <stdint.h>
#include <stddef.h>

#include <optional>

#include "defs.h"
#include "interp.h"

namespace ikura::cmd
{
	struct InterpState;

	namespace properties
	{
		constexpr uint8_t TIMEOUT_NONE          = 0;
		constexpr uint8_t TIMEOUT_PER_USER      = 1;
		constexpr uint8_t TIMEOUT_PER_CHANNEL   = 2;
		constexpr uint8_t TIMEOUT_GLOBAL        = 3;
	};

	struct Command : Serialisable
	{
		virtual ~Command() { }

		std::string getName() const { return this->name; }
		virtual std::optional<interp::Value> run(InterpState* fs, CmdContext& cs) const = 0;

		// because this is static, it needs to exist in the abstract base class too
		static std::optional<Command*> deserialise(Span& buf);

	protected:
		Command(std::string name);

		std::string name;
		uint32_t permissions;       // see defs.h/ikura::permissions
	};

	struct Macro : Command
	{
		Macro(std::string name, ikura::str_view raw_code);

		virtual std::optional<interp::Value> run(InterpState* fs, CmdContext& cs) const override;

		virtual void serialise(Buffer& buf) const override;
		static std::optional<Macro*> deserialise(Span& buf);

		static constexpr uint8_t TYPE_TAG = serialise::TAG_MACRO;

	private:
		// only used when deserialising.
		Macro(std::string name, std::vector<std::string> codewords);

		std::vector<std::string> code;
	};


	void init();
	void processMessage(ikura::str_view user, const Channel* channel, ikura::str_view message);
}
