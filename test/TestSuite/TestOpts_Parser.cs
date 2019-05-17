
using System;



public class Parser {
	const int _EOF = 0;
	const int _a = 1;
	const int _b = 2;
	const int _c = 3;
	const int _d = 4;
	const int _e = 5;
	const int _f = 6;
	const int maxT = 7;

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
			Get();
		} else if (la.kind == 2 || la.kind == 3) {
			if (la.kind == 2) {
				Get();
			}
			Expect(3);
		} else if (la.kind == 0 || la.kind == 5) {
			if (la.kind == 5) {
				Del();
			}
		} else if (la.kind == 4) {
			Get();
			if (la.kind == 4 || la.kind == 5 || la.kind == 6) {
				if (la.kind == 4) {
					Get();
				}
				if (la.kind == 5) {
					Get();
				}
				Expect(6);
			}
		} else SynErr(8);
	}

	void Del() {
		if (la.kind == 5) {
			Get();
		}
	}



	public void Parse() {
		la = new Token();
		la.val = "";		
		Get();
		Test();

    Expect(0);
	}
	
	bool[,] set = {
		{T,x,x,x, x,x,x,x, x}

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
			case 1: s = "a expected"; break;
			case 2: s = "b expected"; break;
			case 3: s = "c expected"; break;
			case 4: s = "d expected"; break;
			case 5: s = "e expected"; break;
			case 6: s = "f expected"; break;
			case 7: s = "??? expected"; break;
			case 8: s = "invalid Test"; break;

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

