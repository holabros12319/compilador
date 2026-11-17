int main() {
    int n = 0;
    int suma = 0;
    cout << "Hasta que numero sumo? ";
    cin >> n;
    for (int i = 1; i <= n; i++) {
        suma = suma + i;
    }
    cout << "La suma es " << suma << '\n';
    return 0;
}
