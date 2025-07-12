# uv run --with sympy adda.py

import sympy as sp
from sympy.plotting import plot

def main():

    m, t, c, d = sp.symbols('m t c d', positive=True, real=True)
    x = sp.symbols('x')
    print("Generating C++ code for Sena function...")

    def h(x):
        return (m + 1)*t - m*x

    def g(x):
        return sp.Piecewise((h(x), x > t), (-h(-x), x < -t), (x, True))    
        
    def l(x):
        return sp.Piecewise((c, -d*(x+1)-1 > c), (-d*x - (d+1), x < -1), (x, x < 1), (d+1 - d*x, (d+1) - d*x > -c), (-c, True))
    
    if False:
        p1 = plot(g(x).subs(m, 8).subs(t, 0.75), (x, -5, 5), title="Sena Function - h", xlabel="x", ylabel="h(x)", show=False)
        p2 = plot(l(x).subs(m, 8).subs(t, 0.75).subs(c, 0.1).subs(d, 1.5), (x, -5, 5), title="Sena Function", xlabel="x", ylabel="f(x)", show=False)
        p1.extend(p2)
        p1.show()

    if True:
        print(g(x))
        print(sp.ccode(g(x)))
        #g_simp = sp.simplify(sp.integrate(g(x), (x, 0, x)))
        g_simp = sp.simplify(sp.integrate(g(x), x))
        print(g_simp)
        p1 = plot(g_simp.subs(m, 8).subs(t, 0.75), (x, -5, 5), title="Sena Function - h", xlabel="x", ylabel="h(x)", show=True)
        print(sp.ccode(g_simp))

    if False:
        # fell over, integrated by hand instead...
        print(sp.ccode(l(x)))
        l_simp = sp.simplify(sp.integrate(l(x).subs(c, 0.1).subs(d, 1.5), (x, 0, x)))
        print(l_simp)
        p2 = plot(l_simp.subs(m, 8).subs(t, 0.75).subs(c, 0.1).subs(d, 1.5), (x, -5, 5), title="Sena Function", xlabel="x", ylabel="f(x)", show=True)
        print(sp.ccode(l_simp))
        print("C++ code generation complete.")

    


if __name__ == "__main__":
    main()