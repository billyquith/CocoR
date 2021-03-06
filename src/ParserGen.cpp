/*-------------------------------------------------------------------------
ParserGen -- Generation of the Recursive Descent Parser
Compiler Generator Coco/R,
Copyright (c) 1990, 2004 Hanspeter Moessenboeck, University of Linz
ported to C++ by Csaba Balazs, University of Szeged
extended by M. Loeberbauer & A. Woess, Univ. of Linz
with improvements by Pat Terry, Rhodes University

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

As an exception, it is allowed to write an extension of Coco/R that is
used as a plugin in non-free software.

If not otherwise stated, any source code generated by Coco/R (other than
Coco/R itself) does not fall under the GNU General Public License.
-------------------------------------------------------------------------*/

#include <ctype.h>
#include "ArrayList.h"
#include "ParserGen.h"
#include "Parser.h"
#include "BitArray.h"
#include "Scanner.h"
#include "Generator.h"

namespace Coco {

void ParserGen::Indent (int n) {
	for (int i = 1; i < n; i++) fprintf(gen, "\t");
}

// AW: use a switch if more than 5 alternatives and none starts with a resolver
bool ParserGen::UseSwitch (Node *p) {
	if (p->typ != Node::alt) return false;
	int nAlts = 0;
	while (p != NULL) {
	  ++nAlts;
	  // must not optimize with switch-statement, if alt uses a resolver expression
	  if (p->sub->typ == Node::rslv) return false;
	  p = p->down;
	}
	return nAlts > 5;
}

int ParserGen::GenNamespaceOpen(const char *nsName) {
	if (nsName == NULL || coco_string_length(nsName) == 0) {
		return 0;
	}
	const int len = coco_string_length(nsName);
	int startPos = 0;
	int nrOfNs = 0;
	do {
		int curLen = coco_string_indexof(nsName + startPos, COCO_CPP_NAMESPACE_SEPARATOR);
		if (curLen == -1) { curLen = len - startPos; }
		char *curNs = coco_string_create(nsName, startPos, curLen);
		fprintf(gen, "namespace %s {\n", curNs);
		coco_string_delete(curNs);
		startPos = startPos + curLen + 1;
		++nrOfNs;
	} while (startPos < len);
	return nrOfNs;
}

void ParserGen::GenNamespaceClose(int nrOfNs) {
	for (int i = 0; i < nrOfNs; ++i) {
		fprintf(gen, "} // namespace\n");
	}
}

void ParserGen::CopySourcePart (Position *pos, int indent) {
	// Copy text described by pos from atg to gen
	const int tabsize = 4;
	int ch, i;
	if (pos != NULL) {
		buffer->SetPos(pos->beg); ch = buffer->Read();
		if (tab->emitLines && pos->line) {
			fprintf(gen, "\n#line %d \"%s\"\n", pos->line, tab->srcName);
		}
		Indent(indent);
		while (buffer->GetPos() <= pos->end) {
			while (ch == CR || ch == LF) {  // eol is either CR or CRLF or LF
				fprintf(gen, "\n"); Indent(indent);
				if (ch == CR) { ch = buffer->Read(); } // skip CR
				if (ch == LF) { ch = buffer->Read(); } // skip LF

				int scol = 0; // source column. How much is the source code indented?
				while (ch == ' ' || ch == '\t') { // skip blanks at beginning of line
					scol += (ch == '\t' ? tabsize - (scol & (tabsize - 1)) : 1);
					ch = buffer->Read();
				}
				const int reqCol = scol - pos->col;
				Indent(reqCol / tabsize);
				for (i = reqCol % tabsize; i > 0; --i)
					fputc(' ', gen);
				if (buffer->GetPos() > pos->end) goto done;
			}
			fprintf(gen, "%c", ch);
			ch = buffer->Read();
		}
		done:
		if (indent > 0) fprintf(gen, "\n");
	}
}

void ParserGen::GenErrorMsg (int errTyp, Symbol *sym) {
	errorNr++;
	const int formatLen = 1000;
	char format[formatLen];
	coco_sprintf(format, formatLen, "\t\t\tcase %d: s = \"", errorNr);
	coco_string_merge(err, format);
	if (errTyp == tErr) {
		if (sym->name[0] == '"') {
			coco_sprintf(format, formatLen, "%s expected", tab->Escape(sym->name));
			coco_string_merge(err, format);
		} else {
			coco_sprintf(format, formatLen, "%s expected", sym->name);
			coco_string_merge(err, format);
		}
	} else if (errTyp == altErr) {
		coco_sprintf(format, formatLen, "invalid %s", sym->name);
		coco_string_merge(err, format);
	} else if (errTyp == syncErr) {
		coco_sprintf(format, formatLen, "this symbol not expected in %s", sym->name);
		coco_string_merge(err, format);
	}
	coco_sprintf(format, formatLen, "\"; break;\n");
	coco_string_merge(err, format);
}

int ParserGen::NewCondSet (BitArray *s) {
	for (int i = 1; i < symSet->Count; i++) // skip symSet[0] (reserved for union of SYNC sets)
		if (Sets::Equals(s, (BitArray*)(*symSet)[i])) return i;
	symSet->Add(s->Clone());
	return symSet->Count - 1;
}

void ParserGen::GenCond (BitArray *s, Node *p) {
	if (p->typ == Node::rslv) CopySourcePart(p->pos, 0);
	else {
		int n = Sets::Elements(s);
		if (n == 0) fprintf(gen, "false"); // happens if an ANY set matches no symbol
		else if (n <= maxTerm) {
			Symbol *sym;
			for (int i=0; i<tab->terminals->Count; i++) {
				sym = (Symbol*)((*(tab->terminals))[i]);
				if ((*s)[sym->n]) {
					fprintf(gen, "la->kind == %s", sym->enumName);
					--n;
					if (n > 0) fprintf(gen, " || ");
				}
			}
		} else
			fprintf(gen, "StartOf(%d)", NewCondSet(s));
	}
}

void ParserGen::PutCaseLabels (BitArray *s) {
	Symbol *sym;
	for (int i=0; i<tab->terminals->Count; i++) {
		sym = (Symbol*)((*(tab->terminals))[i]);
		if ((*s)[sym->n]) fprintf(gen, "case %s: ", sym->enumName);
	}
}

void ParserGen::GenCode (Node *p, int indent, BitArray *isChecked) {
	Node *p2;
	BitArray *s1, *s2;
	while (p != NULL) {
		if (p->typ == Node::nt) {
			Indent(indent);
			fprintf(gen, "%s(", p->sym->name);
			CopySourcePart(p->pos, 0);
			fprintf(gen, ");\n");
		} else if (p->typ == Node::t) {
			Indent(indent);
			// assert: if isChecked[p->sym->n] is true, then isChecked contains only p->sym->n
			if ((*isChecked)[p->sym->n]) fprintf(gen, "Get();\n");
			else fprintf(gen, "Expect(%s);\n", p->sym->enumName);
		} if (p->typ == Node::wt) {
			Indent(indent);
			s1 = tab->Expected(p->next, curSy);
			s1->Or(tab->allSyncSets);
			fprintf(gen, "ExpectWeak(%s, %d);\n", p->sym->enumName, NewCondSet(s1));
		} if (p->typ == Node::any) {
			Indent(indent);
			int acc = Sets::Elements(p->set);
			if (tab->terminals->Count == (acc + 1) || (acc > 0 && Sets::Equals(p->set, isChecked))) {
				// either this ANY accepts any terminal (the + 1 = end of file), or exactly what's allowed here
				fprintf(gen, "Get();\n");
			} else {
				GenErrorMsg(altErr, curSy);
				if (acc > 0) {
					fprintf(gen, "if ("); GenCond(p->set, p); fprintf(gen, ") Get(); else SynErr(%d);\n", errorNr);
				} else fprintf(gen, "SynErr(%d); // ANY node that matches no symbol\n", errorNr);
			}
		} if (p->typ == Node::eps) {	// nothing
		} if (p->typ == Node::rslv) {	// nothing
		} if (p->typ == Node::sem) {
			CopySourcePart(p->pos, indent);
		} if (p->typ == Node::sync) {
			Indent(indent);
			GenErrorMsg(syncErr, curSy);
			s1 = p->set->Clone();
			fprintf(gen, "while (!("); GenCond(s1, p); fprintf(gen, ")) {");
			fprintf(gen, "SynErr(%d); Get();", errorNr); fprintf(gen, "}\n");
		} if (p->typ == Node::alt) {
			s1 = tab->First(p);
			bool equal = Sets::Equals(s1, isChecked);
			bool useSwitch = UseSwitch(p);
			if (useSwitch) { Indent(indent); fprintf(gen, "switch (la->kind) {\n"); }
			p2 = p;
			while (p2 != NULL) {
				s1 = tab->Expected(p2->sub, curSy);
				Indent(indent);
				if (useSwitch) {
					PutCaseLabels(s1); fprintf(gen, "{\n");
				} else if (p2 == p) {
					fprintf(gen, "if ("); GenCond(s1, p2->sub); fprintf(gen, ") {\n");
				} else if (p2->down == NULL && equal) { fprintf(gen, "} else {\n");
				} else {
					fprintf(gen, "} else if (");  GenCond(s1, p2->sub); fprintf(gen, ") {\n");
				}
				GenCode(p2->sub, indent + 1, s1);
				if (useSwitch) {
					Indent(indent); fprintf(gen, "\tbreak;\n");
					Indent(indent); fprintf(gen, "}\n");
				}
				p2 = p2->down;
			}
			Indent(indent);
			if (equal) {
				fprintf(gen, "}\n");
			} else {
				GenErrorMsg(altErr, curSy);
				if (useSwitch) {
					fprintf(gen, "default: SynErr(%d); break;\n", errorNr);
					Indent(indent); fprintf(gen, "}\n");
				} else {
					fprintf(gen, "} "); fprintf(gen, "else SynErr(%d);\n", errorNr);
				}
			}
		} if (p->typ == Node::iter) {
			Indent(indent);
			p2 = p->sub;
			fprintf(gen, "while (");
			if (p2->typ == Node::wt) {
				s1 = tab->Expected(p2->next, curSy);
				s2 = tab->Expected(p->next, curSy);
				fprintf(gen, "WeakSeparator(%d,%d,%d) ", p2->sym->n, NewCondSet(s1), NewCondSet(s2));
				s1 = new BitArray(tab->terminals->Count);  // for inner structure
				if (p2->up || p2->next == NULL) p2 = NULL; else p2 = p2->next;
			} else {
				s1 = tab->First(p2);
				GenCond(s1, p2);
			}
			fprintf(gen, ") {\n");
			GenCode(p2, indent + 1, s1);
			Indent(indent); fprintf(gen, "}\n");
		} if (p->typ == Node::opt) {
			s1 = tab->First(p->sub);
			Indent(indent);
			fprintf(gen, "if ("); GenCond(s1, p->sub); fprintf(gen, ") {\n");
			GenCode(p->sub, indent + 1, s1);
			Indent(indent); fprintf(gen, "}\n");
		}
		if (p->typ != Node::eps && p->typ != Node::sem && p->typ != Node::sync)
			isChecked->SetAll(false);  // = new BitArray(Symbol.terminals.Count);
		if (p->up) break;
		p = p->next;
	}
}

static bool SetEnumName(Symbol *sym) {
	char sn[256];
	bool valid = true;
	if (isalpha(sym->name[0])) {
		coco_sprintf(sn, sizeof(sn), "_%s", sym->name);
	}
	else {
		valid = false;
		coco_sprintf(sn, sizeof(sn), "%d /* %s */", sym->n, sym->name);
	}
	sym->enumName = coco_string_create(sn);
	return valid;
}

void ParserGen::GenTokensHeader() {
	Symbol *sym;
	int i;
	bool isFirst = true;

	fprintf(gen, "\tenum {\n");

	// tokens
	for (i=0; i<tab->terminals->Count; i++) {
		sym = (Symbol*)((*(tab->terminals))[i]);

		const bool valid = SetEnumName(sym);

		if (isFirst) { isFirst = false; }
		else { fprintf(gen, ",\n"); }

		if (valid) {
			fprintf(gen, "\t\t_%s=%d", sym->name, sym->n);
		}
		else {
			fprintf(gen, "\t\t// %s=%d", sym->name, sym->n);
		} 
	}

	// pragmas
	for (i=0; i<tab->pragmas->Count; i++) {
		sym = (Symbol*)((*(tab->pragmas))[i]);
		const bool valid = SetEnumName(sym);
		if (isFirst) { isFirst = false; }
		else { fprintf(gen, ",\n"); }

		if (valid) {
			fprintf(gen, "\t\t%s=%d", sym->enumName, sym->n);
		}
		else {
			fprintf(gen, "\t\t// %s=%d", sym->name, sym->n);
		}
	}

	fprintf(gen, "\n\t};\n");
}

void ParserGen::GenCodePragmas() {
	Symbol *sym;
	for (int i=0; i<tab->pragmas->Count; i++) {
		sym = (Symbol*)((*(tab->pragmas))[i]);
		fprintf(gen, "\t\tif (la->kind == %s) {\n", sym->enumName);
		CopySourcePart(sym->semPos, 4);
		fprintf(gen, "\t\t}\n");
	}
}


void ParserGen::GenProductionsHeader() {
	Symbol *sym;
	for (int i=0; i<tab->nonterminals->Count; i++) {
		sym = (Symbol*)((*(tab->nonterminals))[i]);
		curSy = sym;
		fprintf(gen, "\tvoid %s(", sym->name);
		CopySourcePart(sym->attrPos, 0);
		fprintf(gen, ");\n");
	}
}

void ParserGen::GenProductions() {
	Symbol *sym;
	for (int i=0; i<tab->nonterminals->Count; i++) {
		sym = (Symbol*)((*(tab->nonterminals))[i]);
		curSy = sym;
		fprintf(gen, "void Parser::%s(", sym->name);
		CopySourcePart(sym->attrPos, 0);
		fprintf(gen, ") {\n");
		CopySourcePart(sym->semPos, 2);
		GenCode(sym->graph, 2, new BitArray(tab->terminals->Count));
		fprintf(gen, "}\n"); fprintf(gen, "\n");
	}
}

void ParserGen::InitSets() {
	fprintf(gen, "\tstatic bool set[%d][%d] = {\n", symSet->Count, tab->terminals->Count+1);

	for (int i = 0; i < symSet->Count; i++) {
		BitArray *s = (BitArray*)(*symSet)[i];
		fprintf(gen, "\t\t{");
		int j = 0;
		Symbol *sym;
		for (int k=0; k<tab->terminals->Count; k++) {
			sym = (Symbol*)((*(tab->terminals))[k]);
			if ((*s)[sym->n]) fprintf(gen, "T,"); else fprintf(gen, "x,");
			++j;
			if (j%4 == 0) fprintf(gen, " ");
		}
		if (i == symSet->Count-1) fprintf(gen, "x}\n"); else fprintf(gen, "x},\n");
	}
	fprintf(gen, "\t};\n\n");
}

void ParserGen::WriteParser () {
	Generator g = Generator(tab, errors);
	int oldPos = buffer->GetPos();  // Pos is modified by CopySourcePart
	symSet->Add(tab->allSyncSets);

	fram = g.OpenFrame("Parser.frame");
	gen = g.OpenGen("Parser.h");

	Symbol *sym;
	for (int i=0; i<tab->terminals->Count; i++) {
		sym = (Symbol*)((*(tab->terminals))[i]);
		GenErrorMsg(tErr, sym);
	}

	g.GenCopyright();
	g.SkipFramePart("-->begin");
	g.CopyFramePart("-->headerdef");

	if (usingPos != NULL) {CopySourcePart(usingPos, 0); fprintf(gen, "\n");}
	g.CopyFramePart("-->namespace_open");
	int nrOfNs = GenNamespaceOpen(tab->nsName);

	g.CopyFramePart("-->constantsheader");
	GenTokensHeader();  /* ML 2002/09/07 write the token kinds */
	fprintf(gen, "\tint maxT;\n");
	g.CopyFramePart("-->declarations"); CopySourcePart(tab->semDeclPos, 0);
	g.CopyFramePart("-->productionsheader"); GenProductionsHeader();
	g.CopyFramePart("-->namespace_close");
	GenNamespaceClose(nrOfNs);

	g.CopyFramePart("-->implementation");
	fclose(gen);

	// Source
	gen = g.OpenGen("Parser.cpp");

	g.GenCopyright();
	g.SkipFramePart("-->begin");
	g.CopyFramePart("-->namespace_open");
	nrOfNs = GenNamespaceOpen(tab->nsName);

	g.CopyFramePart("-->pragmas"); GenCodePragmas();
	g.CopyFramePart("-->productions"); GenProductions();
	g.CopyFramePart("-->parseRoot"); fprintf(gen, "\t%s();\n", tab->gramSy->name); if (tab->checkEOF) fprintf(gen, "\tExpect(0);");
	g.CopyFramePart("-->constants");
	fprintf(gen, "\tmaxT = %d;\n", tab->terminals->Count-1);
	g.CopyFramePart("-->initialization"); InitSets();
	g.CopyFramePart("-->errors"); fprintf(gen, "%s", err);
	g.CopyFramePart("-->namespace_close");
	GenNamespaceClose(nrOfNs);
	g.CopyFramePart(NULL);
	fclose(gen);
	buffer->SetPos(oldPos);
}


void ParserGen::WriteStatistics () {
	fprintf(trace, "\n");
	fprintf(trace, "%d terminals\n", tab->terminals->Count);
	fprintf(trace, "%d symbols\n", tab->terminals->Count + tab->pragmas->Count +
	                               tab->nonterminals->Count);
	fprintf(trace, "%d nodes\n", tab->nodes->Count);
	fprintf(trace, "%d sets\n", symSet->Count);
}


ParserGen::ParserGen (Parser *parser) {
	maxTerm = 3;
	CR = '\r';
	LF = '\n';
	tErr = 0;
	altErr = 1;
	syncErr = 2;
	tab = parser->tab;
	errors = parser->errors;
	trace = parser->trace;
	buffer = parser->scanner->buffer;
	errorNr = -1;
	usingPos = NULL;

	symSet = new ArrayList();
	err = NULL;
}

} // namespace
