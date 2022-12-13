templa -- Copy files with replacing filenames and contents

Usage: templa [OPTIONS] source1 ... destination

  source1 ...   Specify file(s) and/or folder(s).
  destination   Specify the destination directory.

Options:
  --replace FROM TO    Replace strings in filename and file contents.
  --exclude "PATTERN"  Exclude the wildcard patterns separated by semicolon.
                       (default: "q;*.bin;.git;.svg;.vs")
  --help               Show this message.
  --version            Show version information.

Contact: Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
