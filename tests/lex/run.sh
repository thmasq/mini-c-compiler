#!/usr/bin/env bash
set -euo pipefail

# Runner de testes do LÉXICO.
# Verifica:
#   (1) Baseline OK/BAD via --lex-only (exit 0 = OK, !=0 = BAD)
#   (2) Sequência de lexemas via --dump-lexemes (asserções finas)
#
# Dicas:
#   BIN=./minicc ./tests/lex/run.sh    # usar binário customizado
#   KEEP_TMP=1 ./tests/lex/run.sh      # manter diretório temporário
#   bash -x ./tests/lex/run.sh         # modo verboso

# ---------- Config ----------
BIN="${BIN:-./minicc}"

TMP="$(mktemp -d -t lexcases.XXXX)"
if [ "${KEEP_TMP:-0}" = "1" ]; then
    echo "# KEEP_TMP=1 — casos em: $TMP"
else
    trap 'rm -rf "$TMP"' EXIT
fi

ok_total=0 ok_pass=0
bad_total=0 bad_pass=0

# ---------- Helpers ----------
# Caso que deve PASSAR (léxico reconhece tudo; exit 0)
run_ok () {
    local name="$1"; shift
    local f="$TMP/${name}.c"
    cat >"$f"
    if "$BIN" --lex-only "$f" >/dev/null 2>&1; then
        echo "PASS (ok):  $name"
        ok_pass=$((ok_pass+1))
    else
        echo "FAIL (ok):  $name  (esperado: exit 0)"
    fi
    ok_total=$((ok_total+1))
}

# Caso que deve FALHAR (erro léxico; exit != 0)
run_bad () {
    local name="$1"; shift
    local f="$TMP/${name}.c"
    cat >"$f"
    if "$BIN" --lex-only "$f" >/dev/null 2>&1; then
        echo "FAIL (bad): $name  (esperado: exit != 0)"
    else
        echo "PASS (bad): $name"
        bad_pass=$((bad_pass+1))
    fi
    bad_total=$((bad_total+1))
}

# Compara a sequência completa de lexemas produzida por --dump-lexemes
# com um arquivo "expected" (.exp).
assert_lexemes () {
    local name="$1"
    local src="$TMP/${name}.c"
    local exp="$TMP/${name}.exp"
    local got
    got="$("$BIN" --dump-lexemes "$src")"
    if diff -u "$exp" <(printf "%s\n" "$got") >/dev/null; then
        echo "PASS (lexemes): $name"
    else
        echo "FAIL (lexemes): $name"
        echo "Expected:"; cat "$exp"
        echo "Got:"; printf "%s\n" "$got"
        exit 1
    fi
}

assert_lexemes_then_error () {
    local name="$1"
    local f="$TMP/${name}.c"
    local exp="$TMP/${name}.exp"

    # Se os arquivos não existirem, lê duas heredocs da chamada; se já existirem, não bloqueia.
    if [ ! -f "$f" ]; then cat >"$f"; fi
    if [ ! -f "$exp" ]; then cat >"$exp"; fi

    # Captura lexemas ignorando o erro final (silencia stderr e não aborta)
    local got
    got="$("$BIN" --dump-lexemes "$f" 2>/dev/null || true)"

    # Compara só o prefixo esperado
    if diff -u "$exp" <(printf "%s\n" "$got" | head -n "$(wc -l <"$exp")") >/dev/null; then
        :
    else
        echo "FAIL (prefix-lexemes): $name"
        echo "Expected prefix:"; cat "$exp"
        echo "Got:"; printf "%s\n" "$got"
        exit 1
    fi

    # O arquivo completo DEVE falhar no léxico
    if "$BIN" --lex-only "$f" >/dev/null 2>&1; then
        echo "FAIL (should error): $name"
        exit 1
    fi
    echo "PASS (lexemes+error): $name"
}



# ---------- Suite de testes ----------
echo "== Gerando e rodando casos de LÉXICO =="

# --------- CASOS OK ---------
# Comentários, int hex (0x2A), char com escape, string com aspas escapadas.
run_ok simple <<'C'
/* comentarios */
int x = 0x2A;
char c = '\n';
const char *s = "a\"b";
int main(){ return 0; }
C

# Inteiros dec/octal/hex, double com expoente; ops aritm., shift, bitwise, lógicos, ++.
run_ok numbers_and_ops <<'C'
int a=0, b=07, c=0xFF; double d=1.23, e=1e-9;
int main(){ int x=1+2-3*4/5%2; x<<=1; x>>=1; x&=3; x|=4; x^=5; if (x&&1||0) x++; return 0; }
C

# Keywords vs identificadores com sufixo.
run_ok ids_keywords <<'C'
auto x; int ifelse=0, main_id=1; return 0;
C

# Escapes em char e string com aspas escapadas.
run_ok strings_chars <<'C'
char c1='\n', c2='\\'; const char *s="hello \"world\"";
int main(){ return 0; }
C

# Sufixos numéricos inteiros e de float.
run_ok numeric_suffixes <<'C'
int a=123u, b=123UL, c=123ll;
float f=1.0f; long double g=1.0L;
int main(){ return 0; }
C

# Hex floats (p-exponent).
run_ok hex_floats <<'C'
double a=0x1.fp+2; double b=0x.8p-1;
int main(){ return (int)a + (int)b; }
C

# Digraphs (<%, %>, <:, :>) => { } [ ].
run_ok digraphs <<'C'
int main()<% int a<:3:> = <:1,2,3:>; return a<:0:>; %>
C

# Ellipsis '...'.
run_ok ellipsis_token <<'C'
int printf(const char*, ...);
int main(){ return 0; }
C

# Ponteiros, '->', ternário, literal .1 (float sem zero à esquerda).
run_ok ptr_and_punct <<'C'
struct S{ int x; };
int main(){ struct S s; struct S *p=&s; p->x=1; return p->x?.1:0; }
C

# Comentário multilinha ignorado.
run_ok multiline_comment <<'C'
/*
linha 1
linha 2
*/
int main(){ return 0; }
C

# Literais wide (L"..." e L'...').
run_ok wide_literals <<'C'
const wchar_t *ws = L"ok";
wchar_t wc = L'a';
int main(){ return 0; }
C

# --------- CASOS BAD ---------
# Caractere inválido.
run_bad invalid_char_at <<'C'
int main(){ return @; }
C

# Byte inválido (fora de string).
run_bad bad_utf8 <<'C'
int main(){ return €; } // caractere inválido fora de string
C

# Barra invertida solta.
run_bad stray_backslash <<'C'
int main(){ return \ ; }
C

# String não terminada.
run_bad unterminated_string <<'C'
int main(){ const char *s = "oops; return 0; }
C

# Char não terminado.
run_bad unterminated_char <<'C'
int main(){ char c = 'x; return 0; }
C

# --------- ASSERTS DE LEXEMAS ---------
# Tokenização básica de "int x=42;"
cat >"$TMP/simple_lexemes.c" <<'SRC'
int x=42;
SRC
cat >"$TMP/simple_lexemes.exp" <<'EXP'
int
x
=
42
;
EXP
assert_lexemes simple_lexemes

# Assinatura de função + bloco com return
cat >"$TMP/func_lexemes.c" <<'SRC'
int gcd(int a, int b){ return a; }
SRC
cat >"$TMP/func_lexemes.exp" <<'EXP'
int
gcd
(
int
a
,
int
b
)
{
return
a
;
}
EXP
assert_lexemes func_lexemes

# ++ seguido de + é VÁLIDO: "x ++ + 1"
cat >"$TMP/plusplus_breaks.c" <<'SRC'
int main(){ int x=0; x+++1; }
SRC
cat >"$TMP/plusplus_breaks.exp" <<'EXP'
int
main
(
)
{
int
x
=
0
;
x
++
+
1
;
}
EXP
assert_lexemes plusplus_breaks

# Prefixo válido seguido de ERRO léxico no fim (barra invertida solta)
# Prefixo válido + erro léxico no fim (barra invertida solta)
cat >"$TMP/prefix_then_error.c" <<'SRC'
int main(){ int x=0; } \
SRC
cat >"$TMP/prefix_then_error.exp" <<'EXP'
int
main
(
)
{
int
x
=
0
;
}
EXP
assert_lexemes_then_error prefix_then_error


# ---------- Resumo ----------
echo
echo "Resumo:"
echo "  OK : $ok_pass / $ok_total"
echo "  BAD: $bad_pass / $bad_total"

# Se algo falhou, sai com erro pra CI
if [ $ok_pass -ne $ok_total ] || [ $bad_pass -ne $bad_total ]; then
    exit 1
fi
