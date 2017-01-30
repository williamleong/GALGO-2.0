#ifndef GENETICALGORITHM_HPP
#define GENETICALGORITHM_HPP

namespace galgo {

//=================================================================================================

template <typename T, int N = 16>
class GeneticAlgorithm
{
   static_assert(std::is_same<float,T>::value || std::is_same<double,T>::value, "variable type can only be float or double, please amend.");
   static_assert(N > 0 && N <= 64, "number of bits cannot be ouside interval [1,64], please amend N");

   template <typename K, int S>
   friend class Population;
   template <typename K, int S>
   friend class Chromosome;

public:
   Population<T,N> pop;       // population of chromosomes
   std::vector<T> lowerBound; // parameters lower bound(s)
   std::vector<T> upperBound; // parameters upper bound(s)
   std::vector<T> initialSet; // initial set parameter(s)

private:
   mutable std::uniform_int_distribution<uint64_t> udistrib; // generate uniform random unsigned long long integers
   static constexpr uint64_t MAXVAL = pow(2, N) - 1;         // maximum unsigned integer value obtained from N bits, evaluated at compile time
  
public: 
   // objective function functor
   std::vector<T> (*Objective)(const std::vector<T>&); 
   // selection method functor initialized to roulette wheel selection                                       
   void (*Selection)(Population<T,N>&) = RWS;  
   // cross-over method functor initialized to 2-point cross-over                                
   void (*CrossOver)(const Population<T,N>&, CHR<T,N>&, CHR<T,N>&) = P2XO;
   // mutation method functor initialized to single-point mutation 
   void (*Mutation)(CHR<T,N>&) = SPM;  
   // adaptation method to constraint(s) functor                                        
   void (*Adaptation)(Population<T,N>& x) = nullptr; 
   // constraint(s) function functor                               
   std::vector<T> (*Constraint)(const std::vector<T>&) = nullptr; 

   T tolerance = 0.0; // terminal condition (inactive if equal to zero)
   T SP = 1.5;        // selective pressure for RSP selection method 
   T covrate = .50;   // cross-over rate
   T mutrate = .05;   // mutation rate                         

   int elitpop = 10;  // elit population size
   int matsize;       // mating pool size, set to popsize by default
   int tntsize = 10;  // tournament size
   int genstep = 10;  // generation step for outputting results
   int precision = 5; // precision for outputting results

   // nullary constructor
   GeneticAlgorithm() {}
   // constructor
   GeneticAlgorithm(std::vector<T> (*Objective)(const std::vector<T>&), int popsize, const std::vector<T>& lowerBound, const std::vector<T>& upperBound, int nbgen, bool output = false);
   // run genetic algorithm
   void run();
   // get best chromosome 
   const CHR<T,N>& result() const;

private:
   int nbgen;     // number of generations
   int nogen = 0; // numero of generation
   int nbparam;   // number of parameters to be estimated
   int popsize;   // population size
   bool output;   // control if results must be outputted

   // check input parameters validity
   bool check() const ;
   // print results for each new generation
   void print() const;
};

/*-------------------------------------------------------------------------------------------------*/
   
// constructor
template <typename T, int N>
GeneticAlgorithm<T,N>::GeneticAlgorithm(std::vector<T> (*Objective)(const std::vector<T>&), int popsize, const std::vector<T>& lowerBound, const std::vector<T>& upperBound, int nbgen, bool output)
{
   this->Objective = Objective;
   this->nbparam = upperBound.size();
   this->popsize = popsize;
   this->matsize = popsize;
   this->lowerBound = lowerBound;
   this->upperBound = upperBound;
   this->nbgen = nbgen;
   this->output = output;
   // initializing uniform distribution generating random unsigned integers
   udistrib = std::uniform_int_distribution<uint64_t>(0, MAXVAL);
}

/*-------------------------------------------------------------------------------------------------*/

// check input parameters validity
template <typename T, int N>
bool GeneticAlgorithm<T,N>::check() const
{
   if (!initialSet.empty()) {
      for (int i = 0; i < nbparam; ++i) {
         if (initialSet[i] < lowerBound[i] || initialSet[i] > upperBound[i]) {
            std::cerr << " Error: in class galgo::GeneticAlgorithm<T>, initial set of parameters (initialSet) cannot be outside [lowerBound,upperBound], please choose a set within these boundaries.\n";
            return false;
         }
      }
   }
   if (lowerBound.size() != upperBound.size()) {
      std::cerr << " Error: in class galgo::GeneticAlgorithm<T>, lower bound (lowerBound) and upper bound (upperBound) must be of same dimension, please adjust.\n";
      return false;
   }
   for (int i = 0; i < nbparam; ++i) {
      if (lowerBound[i] > upperBound[i]) {
         std::cerr << " Error: in class galgo::GeneticAlgorithm<T>, lower bound (lowerBound) cannot be greater than upper bound (upperBound), please amend.\n";
         return false;
      }
   }
   if (SP < 1.0 || SP > 2.0) {
      std::cerr << " Error: in class galgo::GeneticAlgorithm<T>, selective pressure (SP) cannot be outside [1.0,2.0], please choose a real value within this interval.\n";
      return false;
   }
   if (elitpop > popsize || elitpop < 0) {
      std::cerr << " Error: in class galgo::GeneticAlgorithm<T>, elit population (elitpop) cannot outside [0,popsize], please choose an integral value within this interval.\n";
      return false;
   }
   if (covrate < 0.0 || covrate > 1.0) {
      std::cerr << " Error: in class galgo::GeneticAlgorithm<T>, cross-over rate (covrate) cannot outside [0.0,1.0], please choose a real value within this interval.\n";
      return false;
   }
   if (genstep <= 0) {
      std::cerr << " Error: in class galgo::GeneticAlgorithm<T>, generation step (genstep) cannot be <= 0, please choose an integral value > 0.\n";
      return false;
   }
   return true;
}

/*-------------------------------------------------------------------------------------------------*/
   
// run genetic algorithm
template <typename T, int N>
void GeneticAlgorithm<T,N>::run()
{
   // checking input parameters validity
   if (!check()) {
      return;
   } 
 
   // setting adaptation method to default if needed
   if (Constraint != nullptr && Adaptation == nullptr) {
      Adaptation = DAC;
   }

   // initializing population
   pop = Population<T,N>(*this);

   if (output) {
      std::cout << "\n Running Genetic Algorithm...\n";
      std::cout << " ----------------------------\n";
   }

   // creating population
   pop.creation();
   // initializing best result and previous best result
   T bestResult = pop(0)->getTotal();
   T prevBestResult = bestResult;
   // outputting results 
   if (output) print();
    
   // starting population evolution
   for (nogen = 1; nogen <= nbgen; ++nogen) {
      // evolving population
      pop.evolution();
      // getting best current result
      bestResult = pop(0)->getTotal();
      // outputting results
      if (output) print();
      // checking convergence
      if (tolerance != 0.0) {
         if (fabs(bestResult - prevBestResult) < fabs(tolerance)) {
            break;
         }
         prevBestResult = bestResult;
      }
   } 

   // outputting contraint value(s)
   if (Constraint != nullptr) {
      // getting best parameter constraint value(s) 
      std::vector<T> cst = pop(0)->getConstraint(); 
      if (output) {
         std::cout << "\n Constraint\n";
         std::cout << " ----------\n";
         for (int i = 0; i < cst.size(); ++i) {
            std::cout << " C" << std::to_string(i + 1) << "(x) = " << std::setw(6) << std::fixed << std::setprecision(precision) << cst[i] << "\n"; 
         }
         std::cout << "\n"; 
      }
   }   
}

/*-------------------------------------------------------------------------------------------------*/

// get best chromosome
template <typename T, int N>
inline const CHR<T,N>& GeneticAlgorithm<T,N>::result() const
{
   return pop(0);
}

/*-------------------------------------------------------------------------------------------------*/
   
// print results for each new generation
template <typename T, int N>
void GeneticAlgorithm<T,N>::print() const
{
   // getting best parameters from best chromosome
   std::vector<T> bestParam = pop(0)->getParam();
   std::vector<T> bestResult = pop(0)->getResult();

   if (nogen % genstep == 0) {
      std::cout << " Generation = " << std::setw(std::to_string(nbgen).size()) << nogen << " |";
      for (int i = 0; i < nbparam; ++i) {
	      std::cout << " X";
         if (nbparam > 1) {
            std::cout << std::to_string(i + 1);
         }
         std::cout << " ="  << std::setw(10) << std::fixed << std::setprecision(precision) << bestParam[i] << " |";
	   }
      for (int i = 0; i < bestResult.size(); ++i) {
	      std::cout << " F";
         if (bestResult.size() > 1) {
            std::cout << std::to_string(i + 1);
         }
         std::cout << "(x) =" << std::setw(12) << std::fixed << std::setprecision(precision) << bestResult[i];
         if (i < bestResult.size() - 1) {
            std::cout << " |";
         } else {
            std::cout << "\n";
         }
	   }
 
   }
}
   
//=================================================================================================

}

#endif
