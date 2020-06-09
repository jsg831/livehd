
#include "fmt/format.h"
#include "boost/multiprecision/cpp_int.hpp"
#include "absl/types/span.h"
#include "absl/strings/numbers.h"

#include "lbench.hpp"
#include "lrand.hpp"
#include "iassert.hpp"

#include "lconst.hpp"

std::string_view Lconst::skip_underscores(std::string_view txt) const {
  if (txt.empty() || txt[0] != '_')
    return txt;

  for (auto i = 1u; i < txt.size(); ++i) {
    if (txt[i] != '_') {
      return txt.substr(i);
    }
  }
  return txt; // orig if only underscores
}

uint16_t Lconst::read_bits(std::string_view txt) {
  if (txt.empty())
    return 0;
  if (!std::isdigit(txt[0]))
    return 0;

  int tmp=0;
  bool ok = absl::SimpleAtoi(txt, &tmp);
  if (!ok && tmp==0) {
    throw std::runtime_error(fmt::format("ERROR: {} should have the number of bits in decimal after u", txt));
  }
  if (tmp <= 0) {
    throw std::runtime_error(fmt::format("ERROR: {} the number of bits should be positive not {}", txt, tmp));
  }
  if (tmp > 32768) {
    throw std::runtime_error(fmt::format("ERROR: {} the number of bits is too big {}", txt, tmp));
  }

  return tmp;
}

void Lconst::process_ending(std::string_view txt, size_t pos) {
  bool u = (txt[pos] == 'u' || txt[pos] == 'U');
  bool s = (txt[pos] == 's' || txt[pos] == 'S');

  explicit_sign = u || s;
  sign = s;

  if (explicit_sign) {
    pos++;
    if (pos < txt.size()) {
      while(pos < txt.size() && txt[pos] == '_')
        pos++;

      bits = read_bits(txt.substr(pos));
      explicit_bits = true;
    }
  }else{
    throw std::runtime_error(fmt::format("ERROR: {} invalid number format", txt));
  }
}

Lconst::Container Lconst::serialize() const {

  Container v;
  unsigned char c = (explicit_str?0x10:0) | (explicit_sign?0x8:0) | (explicit_bits?0x4:0) | (sign?0x2:0);
  v.emplace_back(c);
  v.emplace_back(bits>>8);
  v.emplace_back(bits & 0xFF);
  boost::multiprecision::export_bits(num, std::back_inserter(v), 8);

  return v;
}

Lconst::Lconst(absl::Span<unsigned char> v) {

  I(v.size()>3); // invalid otherwise

  uint8_t c0  = v[0];
  uint16_t c1 = v[1];
  uint16_t c2 = v[2];

  explicit_str  = (c0 & 0x18)?true:false;
  explicit_sign = (c0 & 0x08)?true:false;
  explicit_bits = (c0 & 0x04)?true:false;
  sign          = (c0 & 0x02)?true:false;

  bits = (c1<<8) | c2;

  auto s = v.subspan(3);
  boost::multiprecision::import_bits(num,s.begin(),s.end());
}

Lconst::Lconst(const Container &v) {

  I(v.size()>3); // invalid otherwise

  uint8_t c0  = v[0];
  uint16_t c1 = v[1];
  uint16_t c2 = v[2];

  explicit_str  = (c0 & 0x10)?true:false;
  explicit_sign = (c0 & 0x08)?true:false;
  explicit_bits = (c0 & 0x04)?true:false;
  sign          = (c0 & 0x02)?true:false;

  bits = (c1<<8) | c2;

  boost::multiprecision::import_bits(num,v.begin()+3,v.end());
}

Lconst::Lconst() {
  explicit_str  = false;
  explicit_sign = false;
  explicit_bits = false;
  sign          = false;
  bits          = 0;
  num           = 0;
}

Lconst::Lconst(uint64_t v) {
  explicit_str  = false;
  explicit_sign = false;
  explicit_bits = false;
  sign          = false;
  num           = v;
  bits          = calc_bits();
}

Lconst::Lconst(uint64_t v, uint16_t b) {
  explicit_str  = false;
  explicit_sign = false;
  explicit_bits = true;
  sign          = false;
  num           = (v & ((1ULL<<b)-1)); // clear upper bits if present
  bits          = b;
  I(calc_bits() <= bits);
}

Lconst::Lconst(std::string_view txt) {

  explicit_str  = false;
  explicit_sign = false;
  explicit_bits = false;
  sign          = false;
  bits          = 0;
  num           = 0;

  if (txt.empty())
    return;

  // Skip leading _ as needed

  if (!txt.empty() && txt[0] == '_') {
    auto txt2 = skip_underscores(txt);

    if (std::isdigit(txt2[0]) || txt2[0] == '-' || txt2[0] == '+') {
      txt = txt2;
    }
  }

  bool negative = false;
  if (txt[0] == '-') {  // negative (may be unsigned -1u)
    negative = true;
    txt = skip_underscores(txt.substr(1)); // skip -
  }

  uint16_t nbits_used = 0;

  int shift_mode = 0;
  if (txt.size() > 2 && txt[0] == '0') {
    shift_mode = char_to_shift_mode[(uint8_t)txt[1]];
  }

  if (shift_mode) {
    if (shift_mode==1) { // 0b binary
      for(const auto ch:txt.substr(2)) {
        if (ch == '0' || ch == '1' || ch == '_')
          continue;
        if (ch == 'u' || ch == 'U' || ch == 's' || ch == 'S')
          break;

        for (int i = txt.size() - 1; i >= 0; --i) {
          if (txt[i] == '_') continue;

          num <<= 8;
          num  += txt[i];
          bits++;
        }
        explicit_str  = true;
        explicit_bits = true;

#if 0
        num <<=8;
        num += (int)'b';

        num <<=8;
        num  += (int)'0';
#endif

        if (negative) {
          num <<= 8;
          num  += (int)'-';
        }

        return;
      }
    }
    I(shift_mode == 1 || shift_mode == 3 || shift_mode==4); // binary or octal or hexa

    for(auto i=2u;i<txt.size();++i) {
      auto v = char_to_val[(uint8_t)txt[i]];
      if (likely(v >= 0)) {
        auto char_sa = char_to_bits[(uint8_t)txt[i]];
        if (unlikely(char_sa > shift_mode)) {
          throw std::runtime_error(fmt::format("ERROR: {} invalid syntax for number {} bits needed for '{}'", txt, char_sa, txt[i]));
          return;
        }
        num = (num << shift_mode) | v;
        if (nbits_used==0) // leading zeroes adds 0 bits, leading 3 adds 2 bits...
          nbits_used = char_sa;
        else
          nbits_used += shift_mode;
        continue;
      }else{
        if (txt[i] == '_') continue;

        process_ending(txt, i);
        break;
      }
    }
  }else if (std::isdigit(txt[0]) || txt[0] == '-' || txt[0] == '+') {
    auto start_i = 0u;
    if (txt.size() > 2 && txt[0] == '0' && (txt[1] == 'd' || txt[1] == 'D')) {
      start_i = 2;
    }
    for (auto i = start_i; i < txt.size(); ++i) {
      auto v = char_to_val[(uint8_t)txt[i]];
      if (likely(v >= 0)) {
        num = (num*10) + v;
      } else {
        if (txt[i] == '_') continue;

        process_ending(txt, i);
        break;
      }
    }

    nbits_used = calc_bits();
  }else{
    // alnum string (not number, still convert to num)
    for(int i=txt.size()-1; i>=0 ;--i) {
      num <<= 8;
      num += txt[i];
    }
    bits          = txt.size() * 8;
    explicit_str  = true;
    explicit_bits = true;
  }

  if (bits && bits < nbits_used) {
    throw std::runtime_error(
        fmt::format("ERROR: {} bits set would truncate value {} which needs {} bits\n", bits, txt, nbits_used));
  }

  if (negative) {
    num  = -num;
    if (!explicit_sign)
      sign = true;
  }

  if (!explicit_bits)
    bits = nbits_used;
}

void Lconst::dump() const {
  if (explicit_str)
    fmt::print("str:{} bits:{}\n", to_string(), bits);
  else
    fmt::print("num:{} sign:{} bits:{} explicit_bits:{} explicit_sign:{}\n", num.str(), sign, bits, explicit_bits, explicit_sign);
}

Lconst Lconst::add(const Lconst &o) const {
  auto max_bits = std::max(bits, o.bits);

  Number res_sum = get_num(max_bits) + o.get_num(max_bits);

  uint16_t res_bits=0u;
  if (res_sum<0)
    res_bits = msb(-res_sum)+1;
  else
    res_bits = msb(res_sum)+1;

  auto res_sign = sign && o.sign;
  if (res_sign)
    res_bits++;

  // explicit kept if both explicit and agree
  auto res_explicit_sign = explicit_sign && o.explicit_sign && sign == o.sign;
  bool res_explicit_bits = false;

  return Lconst(false, res_explicit_sign, res_explicit_bits, res_sign, res_bits, res_sum);
}

std::string Lconst::to_string() const {
  I(explicit_str);

  std::string str;
  Number tmp = num;
  while(tmp) {
    unsigned char ch = static_cast<unsigned char>(tmp & 0xFF);
    str.append(1, ch);
    tmp >>= 8;
  }

  return str;
}

std::string Lconst::fmt() const {

  if (explicit_str) {
    return to_string();
  }

  std::stringstream ss;
  ss << std::hex;
  const auto v = get_num(bits);
  if (v<0)
    ss << -v;
  else
    ss << v;

  std::string str;
  if (is_negative())
    str.append(1,'-');

  str.append("0x");
  str.append(ss.str());

  if (explicit_sign || explicit_bits) {
    if (sign)
      str.append(1,'s');
    else
      str.append(1,'u');
    if (explicit_bits && bits>0) {
      str.append(std::to_string(bits));
      if (bits > 1)
        str.append("bits");
      else
        str.append("bit");
    }
  }

  return str;
}

