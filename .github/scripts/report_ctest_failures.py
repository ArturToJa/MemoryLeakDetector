"""Publish ctest's failed-test output as a GitHub Actions error annotation.

Annotations are readable through the public GitHub API, unlike the raw job
log (which needs authentication), so a CI failure can be diagnosed without
logging in.

usage: report_ctest_failures.py <ctest-output-file> <label>
"""

import sys

# Annotation messages are capped by GitHub; stay comfortably below it.
MAX_CHARS = 30000


def main(log_path, label):
    with open(log_path, errors="replace") as log:
        text = log.read()[:MAX_CHARS]

    # Workflow commands are single-line, so newlines (and the escape
    # character itself) have to be percent-encoded.
    text = text.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")

    print("::error title=ctest failures (%s)::%s" % (label, text))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)

    main(sys.argv[1], sys.argv[2])
