#include "../include/fraction.h"

#include <cmath>
#include <numeric>
#include <regex>
#include <sstream>

big_int gcd(big_int a, big_int b) {
	if (a < 0) a = 0_bi - a;
	if (b < 0) b = 0_bi - b;

	while (b != 0) {
		big_int tmp = b;
		b = a % b;
		a = tmp;
	}

	return a;
}

void fraction::optimise() {
	if (_denominator == 0) throw std::invalid_argument("Denominator cannot be zero");

	if (_numerator == 0) {
		_denominator = 1;
		return;
	}

	big_int divisor = gcd(_numerator, _denominator);
	_numerator /= divisor;
	_denominator /= divisor;
	if (_denominator < 0) {
		_numerator = 0_bi - _numerator;
		_denominator = 0_bi - _denominator;
	}
}


fraction::fraction(const pp_allocator<big_int::value_type> allocator) : _numerator(0, allocator),
																		_denominator(1, allocator) {}

fraction &fraction::operator+=(fraction const &other) & {
	_numerator = _numerator * other._denominator + _denominator * other._numerator;
	_denominator = _denominator * other._denominator;
	optimise();
	return *this;
}

fraction fraction::operator+(fraction const &other) const {
	fraction result = *this;
	result += other;
	return result;
}

fraction &fraction::operator-=(fraction const &other) & {
	_numerator = _numerator * other._denominator - _denominator * other._numerator;
	_denominator = _denominator * other._denominator;
	optimise();
	return *this;
}

fraction fraction::operator-(fraction const &other) const {
	fraction result = *this;
	result -= other;
	return result;
}

fraction &fraction::operator*=(fraction const &other) & {
	_numerator *= other._numerator;
	_denominator *= other._denominator;
	optimise();
	return *this;
}

fraction fraction::operator*(fraction const &other) const {
	fraction result = *this;
	result *= other;
	return result;
}

fraction &fraction::operator/=(fraction const &other) & {
	if (other._numerator == 0) throw std::invalid_argument("Division by zero");

	_numerator *= other._denominator;
	_denominator *= other._numerator;
	optimise();
	return *this;
}

fraction fraction::operator/(fraction const &other) const {
	fraction result = *this;
	result /= other;
	return result;
}

fraction fraction::operator-() const {
	fraction result = *this;
	result._numerator = 0_bi - result._numerator;
	result.optimise();
	return result;
}

bool fraction::operator==(fraction const &other) const noexcept {
	return _numerator == other._numerator && _denominator == other._denominator;
}

std::partial_ordering fraction::operator<=>(const fraction &other) const noexcept {
	big_int l_val = _numerator * other._denominator;
	big_int r_val = _denominator * other._numerator;

	if (l_val < r_val) return std::partial_ordering::less;
	if (l_val > r_val) return std::partial_ordering::greater;
	return std::partial_ordering::equivalent;
}

std::ostream &operator<<(std::ostream &stream, fraction const &obj) {
	return stream << obj.to_string();
}

std::istream &operator>>(std::istream &stream, fraction &obj) {
	std::string input;
	stream >> input;

	std::regex pattern(R"(^([-+]?\d+)(?:/([-+]?\d+))?$)");
	std::smatch match;

	if (!std::regex_match(input, match, pattern)) {
		throw std::invalid_argument("Invalid fraction format");
	}

	big_int numerator(match[1].str(), 10);
	big_int denominator = match[2].matched ? big_int(match[2].str(), 10) : 1;

	obj = fraction(numerator, denominator);
	return stream;
}

std::string fraction::to_string() const {
	std::stringstream string;
	string << _numerator << "/" << _denominator;
	return string.str();
}

fraction fraction::sin(fraction const &epsilon) const {
	auto x = *this;
	fraction result(0, 1);
	fraction term = x;
	int n = 1;
	while (term >= epsilon || -term <= epsilon) {
		result += term;
		term = term * (-x * x);
		term /= fraction((2 * n) * (2 * n + 1), 1);
		n++;
	}

	return result;
}

fraction fraction::cos(fraction const &epsilon) const {
	auto x = *this;
	fraction result(1, 1);
	fraction term(1, 1);
	int n = 1;
	while (true) {
		term = term * (-x * x);
		big_int dem = (2 * n - 1) * (2 * n);
		term /= fraction(dem, 1);
		if (term <= epsilon || -term >= epsilon) break;

		result += term;
		n++;
	}

	return result;
}

fraction fraction::tg(fraction const &epsilon) const {
	auto cos = this->cos(epsilon * epsilon);
	if (cos._numerator == 0) throw std::domain_error("Tangent undefined");

	return this->sin(epsilon * epsilon) / cos;
}

fraction fraction::arcsin(const fraction &epsilon) const {
	if (*this < fraction(-1, 1) || *this > fraction(1, 1)) {
		throw std::domain_error("Arcsin is undefined for |x| > 1");
	}

	fraction x = *this;
	fraction result = fraction(0, 1);
	fraction term = x;
	big_int n = 1;
	fraction x_squar = x * x;

	while (term >= epsilon) {
		result += term;
		fraction num_dem((2_bi * n - 1) * (2_bi * n - 1), (2_bi * n) * (2_bi * n + 1));
		term = term * x_squar * fraction(num_dem);
		n += 1;
	}

	return result;
}

fraction fraction::arccos(const fraction &epsilon) const {
	if (*this < fraction(-1, 1) || *this > fraction(1, 1)) {
		throw std::domain_error("Arccos is undefined for |x| > 1");
	}

	return fraction(1, 2).arcsin(epsilon) * fraction(3, 1) - this->arcsin(epsilon);
}

fraction fraction::ctg(fraction const &epsilon) const {
	auto sine = this->sin(epsilon * epsilon);
	if (sine._numerator == 0) throw std::domain_error("Cotangent undefined");

	return this->cos(epsilon * epsilon) / sine;
}

fraction fraction::sec(fraction const &epsilon) const {
	auto cos = this->cos(epsilon);
	if (cos._numerator == 0) throw std::domain_error("Secant undefined");

	return fraction(1, 1) / cos;
}

fraction fraction::cosec(fraction const &epsilon) const {
	auto sin = this->sin(epsilon);
	if (sin._numerator == 0) throw std::domain_error("Cosecant undefined");

	return fraction(1, 1) / sin;
}

fraction fraction::arctg(fraction const &epsilon) const {
	if (_numerator < 0) return -(-*this).arctg(epsilon);

	if (*this > fraction(1, 1)) {
		return fraction(1, 2) - (fraction(1, 1) / *this).arctg(epsilon);
	}

	fraction result(0, 1);
	fraction term = *this;
	int n = 1;
	while (term > epsilon) {
		result += fraction((n % 2 == 0 ? -1 : 1), n) * term;
		n += 2;
		term *= *this * *this;
	}

	return result;
}

fraction fraction::arcctg(fraction const &epsilon) const {
	if (_numerator == 0) throw std::domain_error("Arccotangent undefined");

	return (fraction(1, 1) / *this).arctg(epsilon);
}

fraction fraction::arcsec(fraction const &epsilon) const {
	if (_numerator == 0) throw std::domain_error("Arcsecant undefined");

	fraction reciprocal = fraction(1, 1) / *this;
	return reciprocal.arccos(epsilon);
}

fraction fraction::arccosec(fraction const &epsilon) const {
	if (_numerator == 0) throw std::domain_error("Arccosecant undefined");

	fraction reciprocal = fraction(1, 1) / *this;
	return reciprocal.arcsin(epsilon);
}

fraction fraction::pow(size_t degree) const {
	if (degree < 0) throw std::invalid_argument("Degree must be positive");
	if (degree == 0) return {1, 1};

	fraction base = *this;
	fraction result(1, 1);
	while (degree > 0) {
		if (degree & 1) result *= base;

		base *= base;
		degree >>= 1;
	}

	return result;
}

fraction fraction::root(size_t degree, fraction const &epsilon) const {
	if (degree <= 0) throw std::invalid_argument("Degree must be more than 0");
	if (degree == 1) return *this;
	if (_numerator < 0 && degree % 2 == 0) throw std::domain_error("Even root of negative number is not real");

	fraction x = *this;
	if (x._numerator < 0) x = -x;

	fraction guess = *this / fraction(degree, 1);
	fraction prev_guess;
	do {
		prev_guess = guess;
		fraction power = guess.pow(degree - 1);
		if (power._numerator == 0) throw std::logic_error("Division by zero in root calculation");

		guess = (fraction(degree - 1, 1) * guess + *this / power) / fraction(degree, 1);
	} while ((guess - prev_guess > epsilon) || (prev_guess - guess > epsilon));

	if (_numerator < 0 && degree % 2 == 1) guess = -guess;

	return guess;
}

fraction fraction::log2(fraction const &epsilon) const {
	if (_numerator <= 0 || _denominator <= 0) throw std::domain_error("Logarithm of non-positive number is undefined");

	return this->ln(epsilon) / fraction(2, 1).ln(epsilon);
}

fraction fraction::ln(fraction const &epsilon) const {
	if (_numerator <= 0 || _denominator <= 0) throw std::domain_error("Natural logarithm of non-positive number is undefined");

	fraction y = (*this - fraction(1, 1)) / (*this + fraction(1, 1));
	fraction y_squard = y * y;
	fraction term = y;
	fraction sum = term;
	int dem = 1;

	while (true) {
		term *= y_squard;
		dem += 2;
		fraction delta = term / fraction(dem, 1);
		if (delta <= epsilon) break;

		sum += delta;
	}

	return sum + fraction(2, 1);
}

fraction fraction::lg(fraction const &epsilon) const {
	if (_numerator <= 0 || _denominator <= 0) throw std::domain_error("Base-10 logarithm of non-positive number is undefined");

	return this->ln(epsilon) / fraction(10, 1).ln(epsilon);
}