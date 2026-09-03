"""Split C++ source into code, comments and string literals, correctly.

Two guards need this and both got it wrong on their own first attempt, in
opposite directions:

  - A line-based "skip lines that start with a comment" skipped every row of
    the i18n table, because each one opens with /* S_KEY */ and then carries
    the strings that matter. The check reported a confident zero.
  - A line-based "everything after //" treated the // in https:// as the start
    of a comment, and reported the domain suffix of a Google API endpoint
    as Portuguese prose.

Both are the same mistake: reading C++ with a regular expression per line. One
small state machine, used by both, is cheaper than two heuristics that fail in
different ways.
"""


def scan(text: str):
    """Yield (kind, line_number, content) for every comment and string literal.

    `kind` is "comment" or "string". Line numbers are 1-based and refer to
    where the token starts. Escapes inside literals are honoured, so a quote
    written as \\" does not end the string early.
    """
    i, n = 0, len(text)
    line = 1
    while i < n:
        c = text[i]

        if c == "\n":
            line += 1
            i += 1

        elif c == '"':
            start_line = line
            i += 1
            buf = []
            while i < n and text[i] != '"':
                if text[i] == "\\" and i + 1 < n:
                    buf.append(text[i])
                    i += 1
                if text[i] == "\n":
                    line += 1
                buf.append(text[i])
                i += 1
            i += 1
            yield "string", start_line, "".join(buf)

        elif c == "'":
            # A character literal, or an apostrophe in a comment we are not in.
            i += 1
            while i < n and text[i] != "'":
                if text[i] == "\\":
                    i += 1
                if i < n and text[i] == "\n":
                    line += 1
                i += 1
            i += 1

        elif text.startswith("//", i):
            start_line = line
            j = text.find("\n", i)
            if j < 0:
                j = n
            yield "comment", start_line, text[i + 2:j]
            i = j

        elif text.startswith("/*", i):
            start_line = line
            j = text.find("*/", i)
            if j < 0:
                j = n
            body = text[i + 2:j]
            line += body.count("\n")
            yield "comment", start_line, body
            i = j + 2

        else:
            i += 1
