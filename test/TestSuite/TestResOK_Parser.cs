
using System;



public class Parser {
	const int _EOF = 0;
	const int maxT = 5;

	const bool T = true;
	const bool x = false;
	const int minErrDist = 2;
	
	public Scanner scanner;
	public Errors  errors;

	public Token t;    // last recognized token
	public Token la;   // lookahead token
	int errDist = minErrDist;



	public Parser(Scanner scanner) {
		this.scanner = scanner;
		errors = new Errors();
	}

	void SynErr (int n) {
		if (errDist >= minErrDist) errors.SynErr(la.line, la.col, n);
		errDist = 0;
	}

	public void SemErr (string msg) {
		if (errDist >= minErrDist) errors.SemErr(t.line, t.col, msg);
		errDist = 0;
	}
	
	void Get () {
		for (;;) {
			t = la;
			la = scanner.Scan();
			if (la.kind <= maxT) { ++errDist; break; }

			la = t;
		}
	}
	
	void Expect (int n) {
		if (la.kind==n) Get(); else { SynErr(n); }
	}
	
	bool StartOf (int s) {
		return set[s, la.kind];
	}
	
	void ExpectWeak (int n, int follow) {
		if (la.kind == n) Get();
		else {
			SynErr(n);
			while (!StartOf(follow)) Get();
		}
	}


	bool WeakSeparator(int n, int syFol, int repFol) {
		int kind = la.kind;
		if (kind == n) {Get(); return true;}
		else if (StartOf(repFol)) {return false;}
		else {
			SynErr(n);
			while (!(set[syFol, kind] || set[repFol, kind] || set[0, kind])) {
				Get();
				kind = la.kind;
			}
			return StartOf(syFol);
		}
	}

	
	void Test() {
		if (la.kind == 1) {
			A();
			B();
			C();
			D();
			E();
			F();
			G();
			H();
		} else if (la.kind == 0 || la.kind == 2 || la.kind == 3) {
			I();
		} else SynErr(6);
	}

	void A() {
		Expect(1);
		while (la.kind == 2) {
			if (true) {
				Expect(2);
				Expect(3);
			}
			Expect(2);
		}
		Expect(3);
	}

	void B() {
		if (la.kind == 1) {
			Get();
		} else if (eee) {
			Expect(2);
		} else if (la.kind == 2) {
		} else SynErr(7);
		Expect(2);
	}

	void C() {
		if (true) {
			Expect(1);
			Expect(2);
		} else if (la.kind == 1) {
			Get();
		} else SynErr(8);
	}

	void D() {
		while (la.kind == 1) {
			if (true) {
				Expect(1);
			} else {
				Get();
				Expect(2);
			}
		}
		Expect(3);
	}

	void E() {
		if (la.kind == 1) {
			Get();
		} else if (la.kind == 3 || la.kind == 4) {
			if (la.kind == 3) {
				if (true) {
					Expect(3);
				} else {
					Get();
					Expect(2);
				}
			}
		} else if (la.kind == 2) {
			Get();
		} else SynErr(9);
		Expect(4);
	}

	void F() {
		while (StartOf(1)) {
			if (true) {
				if (la.kind == 1) {
					Get();
				}
				Expect(2);
			} else if (la.kind == 4 || la.kind == 5) {
				Get();
			} else {
				Get();
			}
		}
		Expect(3);
	}

	void G() {
		while (aaa) {
			Expect(1);
		}
		while (bbb) {
			if (eee) {
				if (la.kind == 1) {
					Get();
				} else if (la.kind == 2) {
					Get();
				} else SynErr(10);
			} else if (la.kind == 2) {
				Get();
			} else SynErr(11);
		}
		Expect(1);
	}

	void H() {
		while (aaa) {
			Expect(1);
		}
		while (la.kind == 1 || la.kind == 2) {
			if (eee) {
				if (la.kind == 1) {
					Get();
				} else if (la.kind == 2) {
					Get();
				} else SynErr(12);
			} else {
				Get();
			}
		}
		Expect(3);
	}

	void I() {
		if (aaa) {
			if (la.kind == 2) {
				Get();
			}
		} else if (la.kind == 0 || la.kind == 3) {
			while (la.kind == 3) {
				Get();
			}
		} else SynErr(13);
	}



	public void Parse() {
		la = new Token();
		la.val = "";		
		Get();
		Test();

    Expect(0);
	}
	
	bool[,] set = {
		{T,x,x,x, x,x,x},
		{x,T,T,x, T,T,x}

	};
} // end Parser


public class Errors {
	public int count = 0;                                    // number of errors detected
	public System.IO.TextWriter errorStream = Console.Out;   // error messages go to this stream
  public string errMsgFormat = "-- line {0} col {1}: {2}"; // 0=line, 1=column, 2=text
  
	public void SynErr (int line, int col, int n) {
		string s;
		switch (n) {
			case 0: s = "EOF expected"; break;
			case 1: s = "\"a\" expected"; break;
			case 2: s = "\"b\" expected"; break;
			case 3: s = "\"c\" expected"; break;
			case 4: s = "\"d\" expected"; break;
			case 5: s = "??? expected"; break;
			case 6: s = "invalid Test"; break;
			case 7: s = "invalid B"; break;
			case 8: s = "invalid C"; break;
			case 9: s = "invalid E"; break;
			case 10: s = "invalid G"; break;
			case 11: s = "invalid G"; break;
			case 12: s = "invalid H"; break;
			case 13: s = "invalid I"; break;

			default: s = "error " + n; break;
		}
		errorStream.WriteLine(errMsgFormat, line, col, s);
		count++;
	}

	public void SemErr (int line, int col, string s) {
		errorStream.WriteLine(errMsgFormat, line, col, s);
		count++;
	}
	
	public void SemErr (string s) {
		errorStream.WriteLine(s);
		count++;
	}
	
	public void Warning (int line, int col, string s) {
		errorStream.WriteLine(errMsgFormat, line, col, s);
	}
	
	public void Warning(string s) {
		errorStream.WriteLine(s);
	}
} // Errors


public class FatalError: Exception {
	public FatalError(string m): base(m) {}
}

