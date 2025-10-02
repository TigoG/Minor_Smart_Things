# Bill of Materials — README

This directory contains the LaTeX Bill of Materials for the Minor Smart Things project.

Compile

Run the following command from the repository root to produce a PDF:

pdflatex -interaction=nonstopmode -output-directory=design design/bom.tex

(Run twice if cross-references or labels are added.)

Assumptions

- Quantities are unknown in the supplied list and are set to '-' in the table. Please update quantities and part numbers as needed.

Recommended packages and encoding

- booktabs, longtable (or tabularx), geometry, inputenc (UTF-8), fontenc, lmodern.

Encoding

- This file and the .tex are UTF-8 encoded; ensure your LaTeX installation supports UTF-8 input.

- Ensure pdflatex is installed (TeX Live / MiKTeX) to run the command above.