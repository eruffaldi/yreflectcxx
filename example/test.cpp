//#include <array>

namespace cff
{
	struct Pippo
	{
		int data[20];
		enum class PippoEnum: int { A,B,C};
		PippoEnum e;

		struct Pippo2
		{
				int w;
		};
	};

	template <class T, int w = 20>
	struct X_
	{
		T A = 0;
		T B[w];
	};

	template <>
	struct X_<float,150>
	{
		typedef float T;
		T A = 0;
		T B[150];
		T C;
	};

	typedef X_<int,10> X;

	template <int k>
	using Xi = X_<double,k>; // unsupported

	using X = X_<int,10>; // int seems invisible (?)

	using X2 = X_<float,150>; // int seems invisible (?)

	using X3 = Xi<20>; // 


}


// ciao
int main(int argc, char const *argv[])
{
	cff::X q;
	cff::X2 q2;
	return sizeof(cff::X2) + q.A;
}

	cff::X3 q2;
