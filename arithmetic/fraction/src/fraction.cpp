#include "../include/fraction.h"
#include <stdexcept>
#include <cmath>

fraction fraction::operator-() const {
    return fraction(-_numerator, _denominator);
}

void fraction::optimise() {
    if (_denominator == big_int(0)) {
        throw std::invalid_argument("Denominator cannot be zero.");
    }

    if (_denominator < big_int(0)) {
        _numerator = _numerator.operator-();
        _denominator = _denominator.operator-();
    }

    big_int a = _numerator.abs();
    big_int b = _denominator.abs();
    while (b != big_int(0)) {
        big_int temp = b;
        b = a % b;
        a = temp;
    }
    big_int gcd = a;

    if (gcd != big_int(0)) {
        _numerator /= gcd;
        _denominator /= gcd;
    }
}

template<std::convertible_to<big_int> f, std::convertible_to<big_int> s>
fraction::fraction(f&& numerator, s&& denominator)
    : _numerator(std::forward<f>(numerator)), _denominator(std::forward<s>(denominator)) {
    optimise();
}

fraction::fraction(pp_allocator<big_int::value_type> alloc)
    : _numerator(0, alloc), _denominator(1, alloc) {}

fraction fraction::abs() const {
    return fraction(_numerator.abs(), _denominator.abs());
}

fraction& fraction::operator+=(const fraction& other) & {
    _numerator = _numerator * other._denominator + other._numerator * _denominator;
    _denominator *= other._denominator;
    optimise();
    return *this;
}

fraction fraction::operator+(const fraction& other) const {
    fraction result(*this);
    result += other;
    return result;
}

fraction& fraction::operator-=(const fraction& other) & {
    _numerator = _numerator * other._denominator - other._numerator * _denominator;
    _denominator *= other._denominator;
    optimise();
    return *this;
}

fraction fraction::operator-(const fraction& other) const {
    fraction result(*this);
    result -= other;
    return result;
}

fraction& fraction::operator*=(const fraction& other) & {
    _numerator *= other._numerator;
    _denominator *= other._denominator;
    optimise();
    return *this;
}

fraction fraction::operator*(const fraction& other) const {
    fraction result(*this);
    result *= other;
    return result;
}

fraction& fraction::operator/=(const fraction& other) & {
    if (other._numerator == big_int(0)) {
        throw std::invalid_argument("Division by zero");
    }
    _numerator *= other._denominator;
    _denominator *= other._numerator;
    optimise();
    return *this;
}

fraction fraction::operator/(const fraction& other) const {
    fraction result(*this);
    result /= other;
    return result;
}

bool fraction::operator==(const fraction& other) const noexcept {
    return (_numerator * other._denominator) == (other._numerator * _denominator);
}

std::partial_ordering fraction::operator<=>(const fraction& other) const noexcept {
    big_int lhs = _numerator * other._denominator;
    big_int rhs = other._numerator * _denominator;
    return lhs <=> rhs;
}

bool fraction::operator>=(const fraction& other) const {
    return (*this > other) || (*this == other);
}

bool fraction::operator>(const fraction& other) const {
    return (_numerator * other._denominator) > (other._numerator * _denominator);
}

std::ostream& operator<<(std::ostream& stream, const fraction& obj) {
    stream << obj._numerator << "/" << obj._denominator.abs();
    return stream;
}

std::istream& operator>>(std::istream& stream, fraction& obj) {
    std::string str;
    stream >> str;

    size_t slash_pos = str.find('/');
    if (slash_pos == std::string::npos) {
        obj._numerator = big_int(str, 10);
        obj._denominator = big_int(1);
    } else {
        std::string num_str = str.substr(0, slash_pos);
        std::string den_str = str.substr(slash_pos + 1);
        obj._numerator = big_int(num_str, 10);
        obj._denominator = big_int(den_str, 10);
    }
    obj.optimise();
    return stream;
}

std::string fraction::to_string() const {
    return _numerator.to_string() + "/" + _denominator.abs().to_string();
}

fraction fraction::sin(const fraction& epsilon) const {
    fraction result(0, 1);
    fraction term = *this;
    int n = 1;

    do {
        result += term;
        fraction x_sq = (*this) * (*this);
        term = term * x_sq;
        term = term / fraction((2*n)*(2*n+1), 1);
        term = term.operator-();
        n++;
    } while (term.abs() >= epsilon);

    return result;
}

fraction fraction::cos(const fraction& epsilon) const {
    fraction result(1, 1);
    fraction term = (*this) * (*this) / fraction(2, 1);
    int n = 1;

    do {
        result -= term;
        fraction x_sq = (*this) * (*this);
        term = term * x_sq;
        term = term / fraction((2*n+1)*(2*n+2), 1);
        n++;
    } while (term.abs() >= epsilon);

    return result;
}

fraction fraction::tg(const fraction& epsilon) const {
    fraction s = this->sin(epsilon);
    fraction c = this->cos(epsilon);

    if (c == fraction(0, 1)) {
        throw std::invalid_argument("Tangent is undefined (cos(x) = 0)");
    }

    return s / c;
}

fraction fraction::ctg(const fraction& epsilon) const {
    fraction s = this->sin(epsilon);
    fraction c = this->cos(epsilon);

    if (s == fraction(0, 1)) {
        throw std::invalid_argument("Cotangent is undefined (sin(x) = 0)");
    }

    return c / s;
}

fraction fraction::sec(const fraction& epsilon) const {
    fraction c = this->cos(epsilon);

    if (c == fraction(0, 1)) {
        throw std::invalid_argument("Secant is undefined (cos(x) = 0)");
    }

    return fraction(1, 1) / c;
}

fraction fraction::cosec(const fraction& epsilon) const {
    fraction s = this->sin(epsilon);

    if (s == fraction(0, 1)) {
        throw std::invalid_argument("Cosecant is undefined (sin(x) = 0)");
    }

    return fraction(1, 1) / s;
}

fraction fraction::pow(size_t degree) const {
    fraction result(1, 1);
    for (size_t i = 0; i < degree; ++i) {
        result *= *this;
    }
    return result;
}

fraction fraction::root(size_t degree, const fraction& epsilon) const {
    if (_numerator < big_int(0) && degree % 2 == 0) {
        throw std::invalid_argument("Even root of negative number");
    }

    fraction x_prev = *this;
    fraction x = (*this + fraction(1, 1)) / fraction(2, 1);

    while ((x - x_prev).abs() >= epsilon) {
        x_prev = x;
        fraction pow_n_1 = x_prev.pow(degree - 1);
        x = (x_prev * fraction(degree - 1, 1) + (*this / pow_n_1)) / fraction(degree, 1);
    }

    return x;
}