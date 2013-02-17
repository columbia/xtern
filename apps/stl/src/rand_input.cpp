#include <iostream>
#include <vector>
#include <algorithm>
#include <iterator>

struct functor {
   functor(double v):val(v) {}
   int operator()() const {
      return ((int)(rand()/(double)RAND_MAX)*val*100);
   }
private:
   double val;
};

int main(int argc, const char** argv) {
   const int size = 10;
   const double range = 3.0f;

   std::vector<double> dvec;
   std::generate_n(std::back_inserter(dvec), size, functor(range));

   // print all
   std::copy(dvec.begin(), dvec.end(), (std::ostream_iterator<int>(std::cout, "\n")));

   return 0;
}
