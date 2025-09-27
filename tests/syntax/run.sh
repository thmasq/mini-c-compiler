# tests/syntax/run.sh
#!/usr/bin/env bash
set -euo pipefail

# Runner de testes da ANÁLISE SINTÁTICA.
# Verifica:
#   (1) Baseline OK/BAD via --parse-only (exit 0 = OK, !=0 = BAD)
#
# Dicas:
#   BIN=./minicc ./tests/syntax/run.sh
#   KEEP_TMP=1 ./tests/syntax/run.sh
#   bash -x ./tests/syntax/run.sh

BIN="${BIN:-./minicc}"

TMP="$(mktemp -d -t synccases.XXXX)"
if [ "${KEEP_TMP:-0}" = "1" ]; then
    echo "# KEEP_TMP=1 — casos em: $TMP"
else
    trap 'rm -rf "$TMP"' EXIT
fi

ok_total=0 ok_pass=0
bad_total=0 bad_pass=0

run_ok () {
    local name="$1"; shift
    local f="$TMP/${name}.c"
    cat >"$f"
    if "$BIN" --parse-only "$f" >/dev/null 2>&1; then
        echo "PASS (ok):  $name"
        ok_pass=$((ok_pass+1))
    else
        echo "FAIL (ok):  $name  (esperado: exit 0)"
    fi
    ok_total=$((ok_total+1))
}

run_bad () {
    local name="$1"; shift
    local f="$TMP/${name}.c"
    cat >"$f"
    if "$BIN" --parse-only "$f" >/dev/null 2>&1; then
        echo "FAIL (bad): $name  (esperado: exit != 0)"
    else
        echo "PASS (bad): $name"
        bad_pass=$((bad_pass+1))
    fi
    bad_total=$((bad_total+1))
}

echo "== Gerando e rodando casos de SINTÁTICA =="

# --------- CASOS OK ---------

# Função mínima com retorno
run_ok simple_func <<'C'
int main(){ return 0; }
C

# Declarações globais e função
run_ok globals_and_func <<'C'
int g, *p, arr[3];
int f(int x){ return x+1; }
int main(){ return f(41); }
C

# if/else aninhado
run_ok nested_if_else <<'C'
int main(){
    int x=0;
    if (x) x=1; else if (!x) x=2; else x=3;
    return x;
}
C

# while, do-while, for
run_ok loops <<'C'
int main(){
    int i=0, s=0;
    while (i<3){ s+=i; i++; }
    do { s++; } while (s<5);
    for (i=0;i<2;i++){ s+=i; }
    return s;
}
C

# switch/case/default
run_ok switch_stmt <<'C'
int main(){
    int x=2, r=0;
    switch(x){
        case 1: r=10; break;
        case 2: r=20; break;
        default: r=30;
    }
    return r;
}
C

# struct/union/enum + uso básico
run_ok sue_basic <<'C'
struct S { int a; };
union U { int x; char c; };
enum E { A, B=3, C };
int main(){
    struct S s; union U u; enum E e = C;
    s.a = 1; u.x = 2;
    return s.a + u.x + e;
}
C

# ponteiros e arrays em parâmetros/retorno
run_ok ptrs_arrays <<'C'
int f(int *p, int a[3]){ return p[0] + a[1]; }
int main(){
    int v=10, a[3]={1,2,3};
    return f(&v, a);
}
C

# --------- CASOS BAD ---------

# Falta ponto-e-vírgula
run_bad missing_semicolon <<'C'
int main(){
    int x=0
    return x;
}
C

# Parêntese não fechado
run_bad unclosed_paren <<'C'
int main( {
    return 0;
}
C

# Chave fechando a menos
run_bad unclosed_brace <<'C'
int main(){
    int x=0;
C

# else solto
run_bad stray_else <<'C'
int main(){
    else return 0;
}
C

# for com cabeçalho inválido (falta ';')
run_bad bad_for_header <<'C'
int main(){
    int i;
    for (i=0 i<10; i++) { }
    return 0;
}
C

# Declaração/assinatura de função malformada
run_bad bad_func_decl <<'C'
int foo(int a,){ return a; }
int main(){ return foo(1); }
C

echo
echo "Resumo:"
echo "  OK : $ok_pass / $ok_total"
echo "  BAD: $bad_pass / $bad_total"

# Falha geral se algo não bateu
if [ $ok_pass -ne $ok_total ] || [ $bad_pass -ne $bad_total ]; then
    exit 1
fi
