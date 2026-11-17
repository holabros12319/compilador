int fib(int n) {
    if (n < 2) { return n; }
    return fib(n - 1) + fib(n - 2);
}

int main() {
    int x = 7;
    float f = 2.5f;
    double d = 1.5;
    char c = 'A';
    bool b = x > 3;
    cout << x << ' ' << f << ' ' << d << ' ' << c << ' ' << b << '\n';
    cout << "fib(15) = " << fib(15) << '\n';
    return 0;
}
