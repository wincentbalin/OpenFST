// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// General weight set and associated semiring operation definitions.

#ifndef FST_LIB_WEIGHT_H_
#define FST_LIB_WEIGHT_H_

#include <cctype>
#include <cmath>
#include <iostream>
#include <sstream>

#include <fst/compat.h>

#include <fst/util.h>


DECLARE_string(fst_weight_parentheses);
DECLARE_string(fst_weight_separator);

namespace fst {

// A semiring is specified by two binary operations Plus and Times and two
// designated elements Zero and One with the following properties:
//
//   Plus: associative, commutative, and has Zero as its identity.
//
//   Times: associative and has identity One, distributes w.r.t. Plus, and
//     has Zero as an annihilator:
//          Times(Zero(), a) == Times(a, Zero()) = Zero().
//
// A left semiring distributes on the left; a right semiring is similarly
// defined.
//
// A Weight class must have binary functions Plus and Times and static member
// functions Zero() and One() and these must form (at least) a left or right
// semiring.
//
// In addition, the following should be defined for a Weight:
//
//   Member: predicate on set membership.
//
//   NoWeight: static member function that returns an element that is
//      not a set member; used to signal an error.
//
//   >>: reads textual representation of a weight.
//
//   <<: prints textual representation of a weight.
//
//   Read(istream &istrm): reads binary representation of a weight.
//
//   Write(ostream &ostrm): writes binary representation of a weight.
//
//   Hash: maps weight to size_t.
//
//   ApproxEqual: approximate equality (for inexact weights)
//
//   Quantize: quantizes w.r.t delta (for inexact weights)
//
//   Divide: for all a, b, c s.t. Times(a, b) == c
//
//     --> b' = Divide(c, a, DIVIDE_LEFT) if a left semiring, b'.Member()
//      and Times(a, b') == c
//     --> a' = Divide(c, b, DIVIDE_RIGHT) if a right semiring, a'.Member()
//      and Times(a', b) == c
//     --> b' = Divide(c, a) = Divide(c, a, DIVIDE_ANY) =
//      Divide(c, a, DIVIDE_LEFT) = Divide(c, a, DIVIDE_RIGHT) if a
//      commutative semiring, b'.Member() and Times(a, b') = Times(b', a) = c
//
//   ReverseWeight: the type of the corresponding reverse weight.
//
//     Typically the same type as Weight for a (both left and right) semiring.
//     For the left string semiring, it is the right string semiring.
//
//   Reverse: a mapping from Weight to ReverseWeight s.t.
//
//     --> Reverse(Reverse(a)) = a
//     --> Reverse(Plus(a, b)) = Plus(Reverse(a), Reverse(b))
//     --> Reverse(Times(a, b)) = Times(Reverse(b), Reverse(a))
//     Typically the identity mapping in a (both left and right) semiring.
//     In the left string semiring, it maps to the reverse string in the right
//     string semiring.
//
//   Properties: specifies additional properties that hold:
//      LeftSemiring: indicates weights form a left semiring.
//      RightSemiring: indicates weights form a right semiring.
//      Commutative: for all a,b: Times(a,b) == Times(b,a)
//      Idempotent: for all a: Plus(a, a) == a.
//      Path: for all a, b: Plus(a, b) == a or Plus(a, b) == b.

// CONSTANT DEFINITIONS

// A representable float near .001.
constexpr float kDelta = 1.0F / 1024.0F;

// For all a, b, c: Times(c, Plus(a, b)) = Plus(Times(c, a), Times(c, b)).
constexpr uint64 kLeftSemiring = 0x0000000000000001ULL;

// For all a, b, c: Times(Plus(a, b), c) = Plus(Times(a, c), Times(b, c)).
constexpr uint64 kRightSemiring = 0x0000000000000002ULL;

constexpr uint64 kSemiring = kLeftSemiring | kRightSemiring;

// For all a, b: Times(a, b) = Times(b, a).
constexpr uint64 kCommutative = 0x0000000000000004ULL;

// For all a: Plus(a, a) = a.
constexpr uint64 kIdempotent = 0x0000000000000008ULL;

// For all a, b: Plus(a, b) = a or Plus(a, b) = b.
constexpr uint64 kPath = 0x0000000000000010ULL;

// For random weight generation: default number of distinct weights.
// This is also used for a few other weight generation defaults.
constexpr size_t kNumRandomWeights = 5;

// Determines direction of division.
enum DivideType {
  DIVIDE_LEFT,   // left division
  DIVIDE_RIGHT,  // right division
  DIVIDE_ANY
};  // division in a commutative semiring

// NATURAL ORDER
//
// By definition:
//
//                 a <= b iff a + b = a
//
// The natural order is a negative partial order iff the semiring is
// idempotent. It is trivially monotonic for plus. It is left
// (resp. right) monotonic for times iff the semiring is left
// (resp. right) distributive. It is a total order iff the semiring
// has the path property.
//
// For more information, see:
//
// Mohri, M. 2002. Semiring framework and algorithms for shortest-distance
// problems, Journal of Automata, Languages and
// Combinatorics 7(3): 321-350, 2002.
//
// We define the strict version of this order below.

template <class W>
class NaturalLess {
 public:
  using Weight = W;

  NaturalLess() {
    // TODO(kbg): Make this a compile-time static_assert once:
    // 1) All weight properties are made constexpr for all weight types.
    // 2) We have a pleasant way to "deregister" this operation for non-path
    //    semirings so an informative error message is produced. The best
    //    solution will probably involve some kind of SFINAE magic.
    if (!(W::Properties() & kIdempotent)) {
      FSTERROR() << "NaturalLess: Weight type is not idempotent: " << W::Type();
    }
  }

  bool operator()(const W &w1, const W &w2) const {
    return (Plus(w1, w2) == w1) && w1 != w2;
  }
};

// Power is the iterated product for arbitrary semirings such that
// Power(w, 0) is One() for the semiring, and Power(w, n) =
// Times(Power(w, n-1), w).

template <class Weight>
Weight Power(Weight w, size_t n) {
  auto result = Weight::One();
  for (auto i = 0; i < n; ++i) result = Times(result, w);
  return result;
}

// General weight converter: raises error.
template <class W1, class W2>
struct WeightConvert {
  W2 operator()(W1 w1) const {
    FSTERROR() << "WeightConvert: Can't convert weight from \"" << W1::Type()
               << "\" to \"" << W2::Type();
    return W2::NoWeight();
  }
};

// Specialized weight converter to self.
template <class W>
struct WeightConvert<W, W> {
  W operator()(W weight) const { return weight; }
};

// General random weight generator: raises error.
template <class W>
struct WeightGenerate {
  W operator()() const {
    FSTERROR() << "WeightGenerate: No random generator for " << W::Type();
    return W::NoWeight();
  }
};

// Helper class for writing textual composite weights.
class CompositeWeightWriter {
 public:
  explicit CompositeWeightWriter(std::ostream &ostrm);

  // Writes open parenthesis to a stream if option selected.
  void WriteBegin();

  // Writes element to a stream.
  template <class T>
  void WriteElement(const T &comp) {
    if (i_++ > 0) ostrm_ << FLAGS_fst_weight_separator[0];
    ostrm_ << comp;
  }

  // Writes close parenthesis to a stream if option selected.
  void WriteEnd();

 private:
  std::ostream &ostrm_;
  int i_;  // Element position.
  CompositeWeightWriter(const CompositeWeightWriter &) = delete;
  CompositeWeightWriter &operator=(const CompositeWeightWriter &) = delete;
};

// Helper class for reading textual composite weights. Elements are separated by
// FLAGS_fst_weight_separator. There must be at least one element per textual
// representation.  FLAGS_fst_weight_parentheses should be set if the composite
// weights themselves contain composite weights to ensure proper parsing.
class CompositeWeightReader {
 public:
  explicit CompositeWeightReader(std::istream &istrm);

  // Reads open parenthesis from a stream if option selected.
  void ReadBegin();

  // Reads element from a stream. The second argument, when true, indicates that
  // this will be the last element (allowing more forgiving formatting of the
  // last element). Returns false when last element is read.
  template <class T>
  bool ReadElement(T *comp, bool last = false);

  // Finalizes reading.
  void ReadEnd();

 private:
  std::istream &istrm_;  // Input stream.
  int c_;                // Last character read, or EOF.
  char separator_;
  bool has_parens_;
  int depth_;  // Weight parentheses depth.
  char open_paren_;
  char close_paren_;

  CompositeWeightReader(const CompositeWeightReader &) = delete;
  CompositeWeightReader &operator=(const CompositeWeightReader &) = delete;
};

template <class T>
inline bool CompositeWeightReader::ReadElement(T *comp, bool last) {
  string s;
  while ((c_ != EOF) && !std::isspace(c_) &&
         (c_ != separator_ || depth_ > 1 || last) &&
         (c_ != close_paren_ || depth_ != 1)) {
    s += c_;
    // If parentheses encountered before separator, they must be matched.
    if (has_parens_ && c_ == open_paren_) {
      ++depth_;
    } else if (has_parens_ && c_ == close_paren_) {
      // Failure on unmatched parentheses.
      if (depth_ == 0) {
        FSTERROR() << "CompositeWeightReader: Unmatched close paren: "
                   << "Is the fst_weight_parentheses flag set correcty?";
        istrm_.clear(std::ios::badbit);
        return false;
      }
      --depth_;
    }
    c_ = istrm_.get();
  }
  if (s.empty()) {
    FSTERROR() << "CompositeWeightReader: Empty element: "
               << "Is the fst_weight_parentheses flag set correcty?";
    istrm_.clear(std::ios::badbit);
    return false;
  }
  std::istringstream istrm(s);
  istrm >> *comp;
  // Skips separator/close parenthesis.
  if (c_ != EOF && !std::isspace(c_)) c_ = istrm_.get();
  // Clears fail bit if just EOF.
  if (c_ == EOF && !istrm_.bad()) istrm_.clear(std::ios::eofbit);
  return c_ != EOF && !std::isspace(c_);
}

}  // namespace fst

#endif  // FST_LIB_WEIGHT_H_
