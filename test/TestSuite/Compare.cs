using System;
using System.IO;

class Compare {
	static int line = 1;
	static int col = 1;
	static string testCaseName;
	const int LF = 10; // line feed
	
	static void WriteLine(string s) {
		FileStream f = new FileStream("log.txt", FileMode.OpenOrCreate);
		StreamWriter w = new StreamWriter(f);
		f.Seek(0, SeekOrigin.End);
		w.WriteLine(s);
		w.Close(); f.Close();
		Console.WriteLine(s);
	}
	
	static void Error() {
		WriteLine("-- failed " + testCaseName + ": line " + line + ", col " + col);
		System.Environment.Exit(1);
	}
	
	public static void Main(string[] arg) {
		FileStream f1 = null;
		FileStream f2 = null;
		if (arg.Length >= 3) {
			try {
				f1 = new FileStream(arg[0], FileMode.Open);
				f2 = new FileStream(arg[1], FileMode.Open);
				testCaseName = arg[2];
				if (arg.Length > 3) {
					long pos = Convert.ToInt64(arg[3]);
					f1.Seek(pos, SeekOrigin.Begin); 
					f2.Seek(pos, SeekOrigin.Begin);
				}
				StreamReader r1 = new StreamReader(f1);
				StreamReader r2 = new StreamReader(f2);
				int c1 = r1.Read();
				int c2 = r2.Read();
				while (c1 >= 0 && c2 >= 0) {
					if (c1 != c2) Error();
					if (c1 == LF) {line++; col = 0;}
					c1 = r1.Read();
					c2 = r2.Read();
					col++;
				}
				if (c1 != c2) Error();
				WriteLine("++ passed " + testCaseName);
				f1.Close();
				f2.Close();
			} catch (IOException e) {
				Console.WriteLine(e.ToString());
				if (f1 != null) f1.Close();
				if (f2 != null) f2.Close();
			}
		} else
			Console.WriteLine("-- invalid number of arguments: fn1 fn2 testName");
	}
	
}