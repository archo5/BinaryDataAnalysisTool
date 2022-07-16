
#include "pch.h"

#include "StructScript.h"


BDSSFastMask MaskFromStartEnd(int sb, int eb)
{
	BDSSFastMask mask = {};
	mask.mask = ~((1ULL << sb) - 1ULL);
	if (eb < 64)
	{
		uint64_t ebm = (1ULL << eb) - 1ULL;
		mask.mask &= ebm;
		mask.extMask = ~ebm;
		mask.lastBit = (1ULL << eb) >> 1;
	}
	return mask;
}


struct BDSSParser
{
	struct Scope
	{
		enum Type
		{
			Struct,
			If,
			Elif,
			Endif,
		};

		Type type;
	};

	BDSScript* script = nullptr;
	ui::StringView it;
	bool oneStruct = false;

	ui::RCHandle<BDSSStruct> stc;
	uint64_t offStc = 0;
	bool isConstantStructSize = true;
	ui::StringView line;
	std::vector<Scope> scopes;

	void Parse()
	{
		while (!it.empty())
		{
			auto origline = it.until_first("\n");
			line = origline.trim();

			ParseLine();

			if (it.size() == origline.size())
			{
				it = {};
				break;
			}
			it = it.substr(origline.size() + 1);
		}

		if (oneStruct)
			FinishStruct();
	}

	bool ConsumeKeyword(ui::StringView kw)
	{
		if (line == kw)
		{
			line = {};
			return true;
		}
		if (line.size() > kw.size() && line.starts_with(kw) && ui::IsSpace(line[kw.size()]))
		{
			line = line.substr(kw.size()).ltrim();
			return true;
		}
		return false;
	}

	bool ConsumePrefix(ui::StringView pfx)
	{
		if (line.take_if_equal(pfx))
		{
			line = line.ltrim();
			return true;
		}
		return false;
	}

	ui::StringView ReadIdent()
	{
		if (line.first_char_is([](char c) { return c == '_' || ui::IsAlpha(c); }))
		{
			auto ident = line.take_while([](char c) { return c == '_' || ui::IsAlphaNum(c); });
			line = line.ltrim();
			return ident;
		}
		// error (unexpected char)
		return {};
	}

	void Error(const std::string& msg)
	{
		printf("ERROR: %s\n", msg.c_str());
	}

	void FinishStruct()
	{
		if (!stc->fixedSize.HasValue() && isConstantStructSize)
			stc->fixedSize = offStc;
		stc = nullptr;
	}

	void ParseLine()
	{
		if (ConsumeKeyword("end"))
		{
			if (scopes.empty())
				return Error("'end': not in a scope");

			auto& S = scopes.back();
			switch (S.type)
			{
			case Scope::Struct:
			{
				FinishStruct();
				break;
			}
			}
			scopes.pop_back();
		}
		else if (ConsumeKeyword("struct"))
		{
			if (oneStruct)
				return Error("cannot define additional structs here");

			stc = new BDSSStruct;
			script->structs.push_back(stc);
			stc->name = ui::to_string(ReadIdent());
			offStc = 0;
			isConstantStructSize = true;

			scopes.push_back({ Scope::Struct });
		}
		else if (ConsumeKeyword("param"))
		{
			BDSSParam p;
			p.name = ui::to_string(ReadIdent());
			p.value = line.take_int64();
			stc->params.push_back(std::move(p));
		}
		else if (ConsumeKeyword("size"))
		{
			stc->sizeExpr.Compile(ui::to_string(ReadIdent()).c_str());
			stc->fixedSize = stc->sizeExpr.GetConstant().StaticCast<uint64_t>();
		}
		else if (ConsumeKeyword("field") || ConsumePrefix("-"))
		{
			ui::RCHandle<BDSSField> fld = new BDSSField;
			stc->fields.push_back(fld);

			// attributes
			while (line.take_if_equal('!'))
			{
				auto attr = ReadIdent();
				if (attr == "be")
					fld->endianness = Endianness::Big;
				else if (attr == "le")
					fld->endianness = Endianness::Little;
				else if (attr == "nullterm")
					fld->readUntil0 = true;
				else if (attr == "excl0")
					fld->excludeZeroes = true;
				else if (attr == "sized")
					fld->countIsMaxSize = true;
				else if (attr == "bitrange")
				{
					ConsumePrefix("(");
					auto start = line.take_int32();
					ConsumePrefix(",");
					auto end = line.take_int32();
					ConsumePrefix(")");
					fld->valueMask = MaskFromStartEnd(start, end);
				}
			}

			// type
			fld->typeName = ui::to_string(ReadIdent());

			// array
			if (line.take_if_equal('['))
			{
				auto expr = line.until_first("]");
				if (expr.size() == line.size())
					line = {};
				else
					line = line.substr(expr.size() + 1).ltrim();
				expr = expr.trim();

				// TODO dummy string
				fld->elemCountExpr.Compile(ui::to_string(expr).c_str());
				fld->fixedElemCount = fld->elemCountExpr.GetConstant().StaticCast<uint64_t>();
			}
			else
				fld->fixedElemCount = 1;

			// name
			if (!line.first_char_equals('@'))
			{
				fld->name = ui::to_string(ReadIdent());
			}

			if (ConsumePrefix("@={"))
			{
				// offset expression
				auto expr = line.until_first("}");
				if (expr.size() == line.size())
					line = {};
				else
					line = line.substr(expr.size() + 1).ltrim();
				expr = expr.trim();

				// TODO dummy string
				fld->offsetExpr.Compile(ui::to_string(expr).c_str());
				fld->fixedOffset = fld->offsetExpr.GetConstant().StaticCast<uint64_t>();
			}
			else if (ConsumePrefix("@"))
			{
				// number offset
				int64_t off = line.take_int64();
				fld->offsetExpr.Compile(std::to_string(off).c_str());
				fld->fixedOffset = fld->offsetExpr.GetConstant().StaticCast<uint64_t>();
				line = line.ltrim();
			}
			else
			{
				fld->fixedOffset = offStc;
			}

			if (fld->fixedOffset.HasValue() && fld->fixedElemCount.HasValue())
			{
				uint64_t size = 0;

				const auto& t = fld->typeName;
				if (t == "char" || t == "i8" || t == "u8") size = 1;
				else if (t == "i16" || t == "u16") size = 2;
				else if (t == "i32" || t == "u32" || t == "f32") size = 4;
				else if (t == "i64" || t == "u64") size = 8;

				offStc = fld->fixedOffset.GetValue() + size * fld->fixedElemCount.GetValue();
			}
			else
				isConstantStructSize = false;
		}
	}
};

bool BDSScript::Parse(ui::StringView s, bool oneStruct)
{
	structs.clear();

	BDSSParser p;
	p.script = this;
	p.oneStruct = oneStruct;
	if (oneStruct)
	{
		p.stc = new BDSSStruct;
		structs.push_back(p.stc);
	}
	p.it = s;

	p.Parse();

	return true;
}
