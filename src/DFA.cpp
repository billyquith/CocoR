/*-------------------------------------------------------------------------
DFA -- Generation of the Scanner Automaton
Compiler Generator Coco/R,
Copyright (c) 1990, 2004 Hanspeter Moessenboeck, University of Linz
extended by M. Loeberbauer & A. Woess, Univ. of Linz
ported to C++ by Csaba Balazs, University of Szeged
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

#include <stdlib.h>
#include <wchar.h>
#include "DFA.h"
#include "Tab.h"
#include "Parser.h"
#include "BitArray.h"
#include "Scanner.h"
#include "Generator.h"

namespace Coco {

//---------- Output primitives
char* DFA::Ch(char _ch) {
    unsigned char ch = (unsigned char)_ch;
	char* format = new char[10];
	if (ch < ' ' || ch >= 127 || ch == '\'' || ch == '\\')
		coco_sprintf(format, 10, "%d\0", (int) ch);
	else
		coco_sprintf(format, 10, "'%c'\0", (int) ch);
	return format;
}

char* DFA::ChCond(char _ch) {
    unsigned char ch = (unsigned char)_ch;
	char* format = new char[20];
	char* res = Ch(ch);
	coco_sprintf(format, 20, "ch == %s\0", res);
	delete [] res;
	return format;
}

void DFA::PutRange(CharSet *s) {
	for (CharSet::Range *r = s->head; r != NULL; r = r->next) {
		if (r->from == r->to) {
			char *from = Ch((char) r->from);
			fprintf(gen, "ch == %s", from);
			delete [] from;
		} else if (r->from == 0) {
			char *to = Ch((char) r->to);
			fprintf(gen, "ch <= %s", to);
			delete [] to;
		} else {
			char *from = Ch((char) r->from);
			char *to = Ch((char) r->to);
			fprintf(gen, "(ch >= %s && ch <= %s)", from, to);
			delete [] from; delete [] to;
		}
		if (r->next != NULL) fprintf(gen, " || ");
	}
}


//---------- State handling

State* DFA::NewState() {
	State *s = new State(); s->nr = ++lastStateNr;
	if (firstState == NULL) firstState = s; else lastState->next = s;
	lastState = s;
	return s;
}

void DFA::NewTransition(State *from, State *to, int typ, int sym, int tc) {
	Target *t = new Target(to);
	Action *a = new Action(typ, sym, tc); a->target = t;
	from->AddAction(a);
	if (typ == Node::clas) curSy->tokenKind = Symbol::classToken;
}

void DFA::CombineShifts() {
	State *state;
	Action *a, *b, *c;
	CharSet *seta, *setb;
	for (state = firstState; state != NULL; state = state->next) {
		for (a = state->firstAction; a != NULL; a = a->next) {
			b = a->next;
			while (b != NULL)
				if (a->target->state == b->target->state && a->tc == b->tc) {
					seta = a->Symbols(tab); setb = b->Symbols(tab);
					seta->Or(setb);
					a->ShiftWith(seta, tab);
					c = b; b = b->next; state->DetachAction(c);
				} else b = b->next;
		}
	}
}

void DFA::FindUsedStates(State *state, BitArray *used) {
	if ((*used)[state->nr]) return;
	used->Set(state->nr, true);
	for (Action *a = state->firstAction; a != NULL; a = a->next)
		FindUsedStates(a->target->state, used);
}

void DFA::DeleteRedundantStates() {
	//State *newState = new State[State::lastNr + 1];
	State **newState = (State**) malloc (sizeof(State*) * (lastStateNr + 1));
	BitArray *used = new BitArray(lastStateNr + 1);
	FindUsedStates(firstState, used);
	// combine equal final states
	for (State *s1 = firstState->next; s1 != NULL; s1 = s1->next) // firstState cannot be final
		if ((*used)[s1->nr] && s1->endOf != NULL && s1->firstAction == NULL && !(s1->ctx))
			for (State *s2 = s1->next; s2 != NULL; s2 = s2->next)
				if ((*used)[s2->nr] && s1->endOf == s2->endOf && s2->firstAction == NULL && !(s2->ctx)) {
					used->Set(s2->nr, false); newState[s2->nr] = s1;
				}

	State *state;
	for (state = firstState; state != NULL; state = state->next)
		if ((*used)[state->nr])
			for (Action *a = state->firstAction; a != NULL; a = a->next)
				if (!((*used)[a->target->state->nr]))
					a->target->state = newState[a->target->state->nr];
	// delete unused states
	lastState = firstState; lastStateNr = 0; // firstState has number 0
	for (state = firstState->next; state != NULL; state = state->next)
		if ((*used)[state->nr]) {state->nr = ++lastStateNr; lastState = state;}
		else lastState->next = state->next;
	free (newState);
	delete used;
}

State* DFA::TheState(Node *p) {
	State *state;
	if (p == NULL) {state = NewState(); state->endOf = curSy; return state;}
	else return p->state;
}

void DFA::Step(State *from, Node *p, BitArray *stepped) {
	if (p == NULL) return;
	stepped->Set(p->n, true);

	if (p->typ == Node::clas || p->typ == Node::chr) {
		NewTransition(from, TheState(p->next), p->typ, p->val, p->code);
	} else if (p->typ == Node::alt) {
		Step(from, p->sub, stepped); Step(from, p->down, stepped);
	} else if (p->typ == Node::iter) {
		if (tab->DelSubGraph(p->sub)) {
			parser->SemErr("contents of {...} must not be deletable");
			return;
		}
		if (p->next != NULL && !((*stepped)[p->next->n])) Step(from, p->next, stepped);
		Step(from, p->sub, stepped);
		if (p->state != from) {
			BitArray *newStepped = new BitArray(tab->nodes->Count);
			Step(p->state, p, newStepped);
			delete newStepped;
		}
	} else if (p->typ == Node::opt) {
		if (p->next != NULL && !((*stepped)[p->next->n])) Step(from, p->next, stepped);
		Step(from, p->sub, stepped);
	}
}

// Assigns a state n.state to every node n. There will be a transition from
// n.state to n.next.state triggered by n.val. All nodes in an alternative
// chain are represented by the same state.
// Numbering scheme:
//  - any node after a chr, clas, opt, or alt, must get a new number
//  - if a nested structure starts with an iteration the iter node must get a new number
//  - if an iteration follows an iteration, it must get a new number
void DFA::NumberNodes(Node *p, State *state, bool renumIter) {
	if (p == NULL) return;
	if (p->state != NULL) return; // already visited;
	if ((state == NULL) || ((p->typ == Node::iter) && renumIter)) state = NewState();
	p->state = state;
	if (tab->DelGraph(p)) state->endOf = curSy;

	if (p->typ == Node::clas || p->typ == Node::chr) {
		NumberNodes(p->next, NULL, false);
	} else if (p->typ == Node::opt) {
		NumberNodes(p->next, NULL, false);
		NumberNodes(p->sub, state, true);
	} else if (p->typ == Node::iter) {
		NumberNodes(p->next, state, true);
		NumberNodes(p->sub, state, true);
	} else if (p->typ == Node::alt) {
		NumberNodes(p->next, NULL, false);
		NumberNodes(p->sub, state, true);
		NumberNodes(p->down, state, renumIter);
	}
}

void DFA::FindTrans (Node *p, bool start, BitArray *marked) {
	if (p == NULL || (*marked)[p->n]) return;
	marked->Set(p->n, true);
	if (start) {
		BitArray *stepped = new BitArray(tab->nodes->Count);
		Step(p->state, p, stepped); // start of group of equally numbered nodes
		delete stepped;
	}

	if (p->typ == Node::clas || p->typ == Node::chr) {
		FindTrans(p->next, true, marked);
	} else if (p->typ == Node::opt) {
		FindTrans(p->next, true, marked); FindTrans(p->sub, false, marked);
	} else if (p->typ == Node::iter) {
		FindTrans(p->next, false, marked); FindTrans(p->sub, false, marked);
	} else if (p->typ == Node::alt) {
		FindTrans(p->sub, false, marked); FindTrans(p->down, false, marked);
	}
}

void DFA::ConvertToStates(Node *p, Symbol *sym) {
	curGraph = p; curSy = sym;
  if (tab->DelGraph(curGraph)) {
    parser->SemErr("token might be empty");
    return;
  }
	NumberNodes(curGraph, firstState, true);
	FindTrans(curGraph, true, new BitArray(tab->nodes->Count));
	if (p->typ == Node::iter) {
		BitArray *stepped = new BitArray(tab->nodes->Count);
		Step(firstState, p, stepped);
		delete stepped;
	}
}

// match string against current automaton; store it either as a fixedToken or as a litToken
void DFA::MatchLiteral(char* s, Symbol *sym) {
	char *subS = coco_string_create(s, 1, coco_string_length(s)-2);
	s = tab->Unescape(subS);
	coco_string_delete(subS);
	int i, len = coco_string_length(s);
	State *state = firstState;
	Action *a = NULL;
	for (i = 0; i < len; i++) { // try to match s against existing DFA
		a = FindAction(state, s[i]);
		if (a == NULL) break;
		state = a->target->state;
	}
	// if s was not totally consumed or leads to a non-final state => make new DFA from it
	if (i != len || state->endOf == NULL) {
		state = firstState; i = 0; a = NULL;
		dirtyDFA = true;
	}
	for (; i < len; i++) { // make new DFA for s[i..len-1]
		State *to = NewState();
		NewTransition(state, to, Node::chr, s[i], Node::normalTrans);
		state = to;
	}
	coco_string_delete(s);
	Symbol *matchedSym = state->endOf;
	if (state->endOf == NULL) {
		state->endOf = sym;
	} else if (matchedSym->tokenKind == Symbol::fixedToken || (a != NULL && a->tc == Node::contextTrans)) {
		// s matched a token with a fixed definition or a token with an appendix that will be cut off
		char format[200];
		coco_sprintf(format, 200, "tokens %s and %s cannot be distinguished", sym->name, matchedSym->name);
		parser->SemErr(format);
	} else { // matchedSym == classToken || classLitToken
		matchedSym->tokenKind = Symbol::classLitToken;
		sym->tokenKind = Symbol::litToken;
	}
}

void DFA::SplitActions(State *state, Action *a, Action *b) {
	Action *c; CharSet *seta, *setb, *setc;
	seta = a->Symbols(tab); setb = b->Symbols(tab);
	if (seta->Equals(setb)) {
		a->AddTargets(b);
		state->DetachAction(b);
	} else if (seta->Includes(setb)) {
		setc = seta->Clone(); setc->Subtract(setb);
		b->AddTargets(a);
		a->ShiftWith(setc, tab);
	} else if (setb->Includes(seta)) {
		setc = setb->Clone(); setc->Subtract(seta);
		a->AddTargets(b);
		b->ShiftWith(setc, tab);
	} else {
		setc = seta->Clone(); setc->And(setb);
		seta->Subtract(setc);
		setb->Subtract(setc);
		a->ShiftWith(seta, tab);
		b->ShiftWith(setb, tab);
		c = new Action(0, 0, Node::normalTrans);  // typ and sym are set in ShiftWith
		c->AddTargets(a);
		c->AddTargets(b);
		c->ShiftWith(setc, tab);
		state->AddAction(c);
	}
}

bool DFA::Overlap(Action *a, Action *b) {
	CharSet *seta, *setb;
	if (a->typ == Node::chr)
		if (b->typ == Node::chr) return (a->sym == b->sym);
		else {setb = tab->CharClassSet(b->sym);	return setb->Get(a->sym);}
	else {
		seta = tab->CharClassSet(a->sym);
		if (b->typ == Node::chr) return seta->Get(b->sym);
		else {setb = tab->CharClassSet(b->sym); return seta->Intersects(setb);}
	}
}

bool DFA::MakeUnique(State *state) { // return true if actions were split
	bool changed = false;
	for (Action *a = state->firstAction; a != NULL; a = a->next)
		for (Action *b = a->next; b != NULL; b = b->next)
			if (Overlap(a, b)) {
				SplitActions(state, a, b);
				changed = true;
			}
	return changed;
}

void DFA::MeltStates(State *state) {
	bool changed, ctx;
	BitArray *targets;
	Symbol *endOf;
	for (Action *action = state->firstAction; action != NULL; action = action->next) {
		if (action->target->next != NULL) {
			GetTargetStates(action, targets, endOf, ctx);
			Melted *melt = StateWithSet(targets);
			if (melt == NULL) {
				State *s = NewState(); s->endOf = endOf; s->ctx = ctx;
				for (Target *targ = action->target; targ != NULL; targ = targ->next)
					s->MeltWith(targ->state);
				do {changed = MakeUnique(s);} while (changed);
				melt = NewMelted(targets, s);
			}
			action->target->next = NULL;
			action->target->state = melt->state;
		}
	}
}

void DFA::FindCtxStates() {
	for (State *state = firstState; state != NULL; state = state->next)
		for (Action *a = state->firstAction; a != NULL; a = a->next)
			if (a->tc == Node::contextTrans) a->target->state->ctx = true;
}

void DFA::MakeDeterministic() {
	State *state;
	bool changed;
	lastSimState = lastState->nr;
	maxStates = 2 * lastSimState; // heuristic for set size in Melted.set
	FindCtxStates();
	for (state = firstState; state != NULL; state = state->next)
		do {changed = MakeUnique(state);} while (changed);
	for (state = firstState; state != NULL; state = state->next)
		MeltStates(state);
	DeleteRedundantStates();
	CombineShifts();
}

void DFA::PrintStates() {
	fprintf(trace, "\n");
	fprintf(trace, "---------- states ----------\n");
	for (State *state = firstState; state != NULL; state = state->next) {
		bool first = true;
		if (state->endOf == NULL) fprintf(trace, "               ");
		else {
			char *paddedName = tab->Name(state->endOf->name);
			fprintf(trace, "E(%12s)", paddedName);
			coco_string_delete(paddedName);
		}
		fprintf(trace, "%3d:", state->nr);
		if (state->firstAction == NULL) fprintf(trace, "\n");
		for (Action *action = state->firstAction; action != NULL; action = action->next) {
			if (first) {fprintf(trace, " "); first = false;} else fprintf(trace, "                    ");

			if (action->typ == Node::clas) fprintf(trace, "%s", ((CharClass*)(*tab->classes)[action->sym])->name);
			else fprintf(trace, "%3s", Ch((char)action->sym));
			for (Target *targ = action->target; targ != NULL; targ = targ->next) {
				fprintf(trace, "%3d", targ->state->nr);
			}
			if (action->tc == Node::contextTrans) fprintf(trace, " context\n"); else fprintf(trace, "\n");
		}
	}
	fprintf(trace, "\n---------- character classes ----------\n");
	tab->WriteCharClasses();
}

//---------------------------- actions --------------------------------

Action* DFA::FindAction(State *state, char ch) {
	for (Action *a = state->firstAction; a != NULL; a = a->next)
		if (a->typ == Node::chr && ch == a->sym) return a;
		else if (a->typ == Node::clas) {
			CharSet *s = tab->CharClassSet(a->sym);
			if (s->Get(ch)) return a;
		}
	return NULL;
}


void DFA::GetTargetStates(Action *a, BitArray* &targets, Symbol* &endOf, bool &ctx) {
	// compute the set of target states
	targets = new BitArray(maxStates); endOf = NULL;
	ctx = false;
	for (Target *t = a->target; t != NULL; t = t->next) {
		int stateNr = t->state->nr;
		if (stateNr <= lastSimState) { targets->Set(stateNr, true); }
		else { targets->Or(MeltedSet(stateNr)); }
		if (t->state->endOf != NULL) {
			if (endOf == NULL || endOf == t->state->endOf) {
				endOf = t->state->endOf;
			}
			else {
				printf("Tokens %s and %s cannot be distinguished\n", endOf->name, t->state->endOf->name);
				errors->count++;
			}
		}
		if (t->state->ctx) {
			ctx = true;
			// The following check seems to be unnecessary. It reported an error
			// if a symbol + context was the prefix of another symbol, e.g.
			//   s1 = "a" "b" "c".
			//   s2 = "a" CONTEXT("b").
			// But this is ok.
			// if (t.state.endOf != null) {
			//   Console.WriteLine("Ambiguous context clause");
			//	 Errors.count++;
			// }
		}
	}
}


//------------------------- melted states ------------------------------


Melted* DFA::NewMelted(BitArray *set, State *state) {
	Melted *m = new Melted(set, state);
	m->next = firstMelted; firstMelted = m;
	return m;

}

BitArray* DFA::MeltedSet(int nr) {
	Melted *m = firstMelted;
	while (m != NULL) {
		if (m->state->nr == nr) return m->set; else m = m->next;
	}
	//Errors::Exception("-- compiler error in Melted::Set");
	//throw new Exception("-- compiler error in Melted::Set");
	return NULL;
}

Melted* DFA::StateWithSet(BitArray *s) {
	for (Melted *m = firstMelted; m != NULL; m = m->next)
		if (Sets::Equals(s, m->set)) return m;
	return NULL;
}


//------------------------ comments --------------------------------

char* DFA::CommentStr(Node *p) {
	StringBuilder s = StringBuilder();
	while (p != NULL) {
		if (p->typ == Node::chr) {
			s.Append((char)p->val);
		} else if (p->typ == Node::clas) {
			CharSet *set = tab->CharClassSet(p->val);
			if (set->Elements() != 1) parser->SemErr("character set contains more than 1 character");
			s.Append((char) set->First());
		}
		else parser->SemErr("comment delimiters may not be structured");
		p = p->next;
	}
	if (s.GetLength() == 0 || s.GetLength() > 2) {
		parser->SemErr("comment delimiters must be 1 or 2 characters long");
		s = StringBuilder("?");
	}
	return s.ToString();
}


void DFA::NewComment(Node *from, Node *to, bool nested) {
	Comment *c = new Comment(CommentStr(from), CommentStr(to), nested);
	c->next = firstComment; firstComment = c;
}


//------------------------ scanner generation ----------------------

void DFA::GenComBody(Comment *com) {
	fprintf(gen, "\t\tfor(;;) {\n");

	char* res = ChCond(com->stop[0]);
	fprintf(gen, "\t\t\tif (%s) ", res);
	fprintf(gen, "{\n");
	delete [] res;

	if (coco_string_length(com->stop) == 1) {
		fprintf(gen, "\t\t\t\tlevel--;\n");
		fprintf(gen, "\t\t\t\tif (level == 0) { oldEols = line - line0; NextCh(); return true; }\n");
		fprintf(gen, "\t\t\t\tNextCh();\n");
	} else {
		fprintf(gen, "\t\t\t\tNextCh();\n");
		char* res = ChCond(com->stop[1]);
		fprintf(gen, "\t\t\t\tif (%s) {\n", res);
		delete [] res;
		fprintf(gen, "\t\t\t\t\tlevel--;\n");
		fprintf(gen, "\t\t\t\t\tif (level == 0) { oldEols = line - line0; NextCh(); return true; }\n");
		fprintf(gen, "\t\t\t\t\tNextCh();\n");
		fprintf(gen, "\t\t\t\t}\n");
	}
	if (com->nested) {
			fprintf(gen, "\t\t\t}");
			char* res = ChCond(com->start[0]);
			fprintf(gen, " else if (%s) ", res);
			delete [] res;
			fprintf(gen, "{\n");
		if (coco_string_length(com->stop) == 1)
			fprintf(gen, "\t\t\t\tlevel++; NextCh();\n");
		else {
			fprintf(gen, "\t\t\t\tNextCh();\n");
			char* res = ChCond(com->start[1]);
			fprintf(gen, "\t\t\t\tif (%s) ", res);
			delete [] res;
			fprintf(gen, "{\n");
			fprintf(gen, "\t\t\t\t\tlevel++; NextCh();\n");
			fprintf(gen, "\t\t\t\t}\n");
		}
	}
	fprintf(gen, "\t\t\t} else if (ch == buffer->EoF) return false;\n");
	fprintf(gen, "\t\t\telse NextCh();\n");
	fprintf(gen, "\t\t}\n");
}

void DFA::GenCommentHeader(Comment *com, int i) {
	fprintf(gen, "\tbool Comment%d();\n", i);
}

void DFA::GenComment(Comment *com, int i) {
	fprintf(gen, "\n");
	fprintf(gen, "bool Scanner::Comment%d() ", i);
	fprintf(gen, "{\n");
	fprintf(gen, "\tint level = 1, pos0 = pos, line0 = line, col0 = col, charPos0 = charPos;\n");
	if (coco_string_length(com->start) == 1) {
		fprintf(gen, "\tNextCh();\n");
		GenComBody(com);
	} else {
		fprintf(gen, "\tNextCh();\n");
		char* res = ChCond(com->start[1]);
		fprintf(gen, "\tif (%s) ", res);
		delete [] res;
		fprintf(gen, "{\n");

		fprintf(gen, "\t\tNextCh();\n");
		GenComBody(com);

		fprintf(gen, "\t} else {\n");
		fprintf(gen, "\t\tbuffer->SetPos(pos0); NextCh(); line = line0; col = col0; charPos = charPos0;\n");
		fprintf(gen, "\t}\n");
		fprintf(gen, "\treturn false;\n");
	}
	fprintf(gen, "}\n");
}

char* DFA::SymName(Symbol *sym) { // real name value is stored in Tab.literals
	if (('a'<=sym->name[0] && sym->name[0]<='z') ||
		('A'<=sym->name[0] && sym->name[0]<='Z')) { //Char::IsLetter(sym->name[0])

		Iterator *iter = tab->literals->GetIterator();
		while (iter->HasNext()) {
			DictionaryEntry *e = iter->Next();
			if (e->val == sym) { return e->key; }
		}
	}
	return sym->name;
}

void DFA::GenLiterals () {
	Symbol *sym;

	ArrayList *ts[2];
	ts[0] = tab->terminals;
	ts[1] = tab->pragmas;

	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < ts[i]->Count; j++) {
			sym = (Symbol*) ((*(ts[i]))[j]);
			if (sym->tokenKind == Symbol::litToken) {
				char* name = coco_string_create(SymName(sym));
				if (ignoreCase) {
					char *oldName = name;
					name = coco_string_create_lower(name);
					coco_string_delete(oldName);
				}
				// sym.name stores literals with quotes, e.g. "\"Literal\""

				fprintf(gen, "\tkeywords.set(");
				// write keyword, escape non printable characters
				for (int k = 0; name[k] != '\0'; k++) {
					char c = name[k];
					fprintf(gen, (c >= 32 && c <= 127) ? "%c" : "\\x%04x", c);
				}
				fprintf(gen, ", %d);\n", sym->n);

				coco_string_delete(name);
			}
		}
	}
}

int DFA::GenNamespaceOpen(const char *nsName) {
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

void DFA::GenNamespaceClose(int nrOfNs) {
	for (int i = 0; i < nrOfNs; ++i) {
		fprintf(gen, "} // namespace\n");
	}
}

void DFA::CheckLabels() {
	int i;
	State *state;
	Action *action;

	for (i=0; i < lastStateNr+1; i++) {
		existLabel[i] = false;
	}

	for (state = firstState->next; state != NULL; state = state->next) {
		for (action = state->firstAction; action != NULL; action = action->next) {
			existLabel[action->target->state->nr] = true;
		}
	}
}

void DFA::WriteState(State *state) {
	Symbol *endOf = state->endOf;
	fprintf(gen, "\t\tcase %d:\n", state->nr);
	if (existLabel[state->nr])
		fprintf(gen, "\t\t\tcase_%d:\n", state->nr);

	if (endOf != NULL && state->firstAction != NULL) {
		fprintf(gen, "\t\t\trecEnd = pos; recKind = %d;\n", endOf->n);
	}
	bool ctxEnd = state->ctx;

	for (Action *action = state->firstAction; action != NULL; action = action->next) {
		if (action == state->firstAction) fprintf(gen, "\t\t\tif (");
		else fprintf(gen, "\t\t\telse if (");
		if (action->typ == Node::chr) {
			char* res = ChCond((char)action->sym);
			fprintf(gen, "%s", res);
			delete [] res;
		} else PutRange(tab->CharClassSet(action->sym));
		fprintf(gen, ") {");

		if (action->tc == Node::contextTrans) {
			fprintf(gen, "apx++; "); ctxEnd = false;
		} else if (state->ctx)
			fprintf(gen, "apx = 0; ");
		fprintf(gen, "AddCh(); goto case_%d;", action->target->state->nr);
		fprintf(gen, "}\n");
	}
	if (state->firstAction == NULL)
		fprintf(gen, "\t\t\t{");
	else
		fprintf(gen, "\t\t\telse {");
	if (ctxEnd) { // final context state: cut appendix
		fprintf(gen, "\n");
		fprintf(gen, "\t\t\t\ttlen -= apx;\n");
		fprintf(gen, "\t\t\t\tSetScannerBehindT();");

		fprintf(gen, "\t\t\t\tbuffer->SetPos(t->pos); NextCh(); line = t->line; col = t->col;\n");
		fprintf(gen, "\t\t\t\tfor (int i = 0; i < tlen; i++) NextCh();\n");
		fprintf(gen, "\t\t\t\t");
	}
	if (endOf == NULL) {
		fprintf(gen, "goto case_0;}\n");
	} else {
		fprintf(gen, "t->kind = %d; ", endOf->n);
		if (endOf->tokenKind == Symbol::classLitToken) {
			if (ignoreCase) {
				fprintf(gen, "char *literal = coco_string_create_lower(tval, 0, tlen); t->kind = keywords.get(literal, t->kind); coco_string_delete(literal); break;}\n");
			} else {
				fprintf(gen, "char *literal = coco_string_create(tval, 0, tlen); t->kind = keywords.get(literal, t->kind); coco_string_delete(literal); break;}\n");
			}
		} else {
			fprintf(gen, "break;}\n");
		}
	}
}

void DFA::WriteStartTab() {
	bool firstRange = true;
	for (Action *action = firstState->firstAction; action != NULL; action = action->next) {
		int targetState = action->target->state->nr;
		if (action->typ == Node::chr) {
			fprintf(gen, "\tstart.set(%d, %d);\n", action->sym, targetState);
		} else {
			CharSet *s = tab->CharClassSet(action->sym);
			for (CharSet::Range *r = s->head; r != NULL; r = r->next) {
				if (firstRange) {
					firstRange = false;
					fprintf(gen, "\tint i;\n");
				}
				fprintf(gen, "\tfor (i = %d; i <= %d; ++i) start.set(i, %d);\n", r->from, r->to, targetState);
			}
		}
	}
	fprintf(gen, "\t\tstart.set(Buffer::EoF, -1);\n");
}

void DFA::WriteScanner() {
	Generator g = Generator(tab, errors);
	fram = g.OpenFrame("Scanner.frame");
	gen = g.OpenGen("Scanner.h");
	if (dirtyDFA) MakeDeterministic();

	// Header
	g.GenCopyright();
	g.SkipFramePart("-->begin");

	g.CopyFramePart("-->namespace_open");
	int nrOfNs = GenNamespaceOpen(tab->nsName);

	g.CopyFramePart("-->casing0");
	if (ignoreCase) {
		fprintf(gen, "\twchar_t valCh;       // current input character (for token.val)\n");
	}
	g.CopyFramePart("-->commentsheader");
	Comment *com = firstComment;
	int cmdIdx = 0;
	while (com != NULL) {
		GenCommentHeader(com, cmdIdx);
		com = com->next; cmdIdx++;
	}

	g.CopyFramePart("-->namespace_close");
	GenNamespaceClose(nrOfNs);

	g.CopyFramePart("-->implementation");
	fclose(gen);

	// Source
	gen = g.OpenGen("Scanner.cpp");
	g.GenCopyright();
	g.SkipFramePart("-->begin");
	g.CopyFramePart("-->namespace_open");
	nrOfNs = GenNamespaceOpen(tab->nsName);

	g.CopyFramePart("-->declarations");
	fprintf(gen, "\tmaxT = %d;\n", tab->terminals->Count - 1);
	fprintf(gen, "\tnoSym = %d;\n", tab->noSym->n);
	WriteStartTab();
	GenLiterals();

	g.CopyFramePart("-->initialization");
	g.CopyFramePart("-->casing1");
	if (ignoreCase) {
		fprintf(gen, "\t\tvalCh = ch;\n");
		fprintf(gen, "\t\tif ('A' <= ch && ch <= 'Z') ch = ch - 'A' + 'a'; // ch.ToLower()");
	}
	g.CopyFramePart("-->casing2");
	fprintf(gen, "\t\ttval[tlen++] = (char)");
	if (ignoreCase) fprintf(gen, "valCh;"); else fprintf(gen, "ch;");

	g.CopyFramePart("-->comments");
	com = firstComment; cmdIdx = 0;
	while (com != NULL) {
		GenComment(com, cmdIdx);
		com = com->next; cmdIdx++;
	}

	g.CopyFramePart("-->scan1");
	fprintf(gen, "\t\t\t");
	if (tab->ignored->Elements() > 0) { PutRange(tab->ignored); } else { fprintf(gen, "false"); }

	g.CopyFramePart("-->scan2");
	if (firstComment != NULL) {
		fprintf(gen, "\tif (");
		com = firstComment; cmdIdx = 0;
		while (com != NULL) {
			char* res = ChCond(com->start[0]);
			fprintf(gen, "(%s && Comment%d())", res, cmdIdx);
			delete [] res;
			if (com->next != NULL) {
				fprintf(gen, " || ");
			}
			com = com->next; cmdIdx++;
		}
		fprintf(gen, ") return NextToken();");
	}
	if (hasCtxMoves) { fprintf(gen, "\n"); fprintf(gen, "\tint apx = 0;"); } /* pdt */
	g.CopyFramePart("-->scan3");

	/* CSB 02-10-05 check the Labels */
	existLabel = new bool[lastStateNr+1];
	CheckLabels();
	for (State *state = firstState->next; state != NULL; state = state->next)
		WriteState(state);
	delete [] existLabel;

	g.CopyFramePart("-->namespace_close");
	GenNamespaceClose(nrOfNs);

	g.CopyFramePart(NULL);
	fclose(gen);
}

DFA::DFA(Parser *parser) {
	this->parser = parser;
	tab = parser->tab;
	errors = parser->errors;
	trace = parser->trace;
	firstState = NULL; lastState = NULL; lastStateNr = -1;
	firstState = NewState();
	firstMelted = NULL; firstComment = NULL;
	ignoreCase = false;
	dirtyDFA = false;
	hasCtxMoves = false;
}

}; // namespace
