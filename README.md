# SRProxy

(Please also see [the general CAFAna `README`](https://github.com/cafana) for more information.)

`SRProxy` is a toolkit for fast reads of `StandardRecord` objects from ROOT files.
It can read two kinds of files:
* "Structured" (or traditional) CAFs, in which there is one `StandardRecord` object per entry
* "Flat" CAFs, in which a `StandardRecord` object is 'flattened' during serialization into basic ROOT types, 
  and the structure is maintained in the branch names only.

Such CAFs are written by "CAF-maker" software maintained by the experiments that use CAFs as their analysis files.

When used, `SRProxy` provides automatic compilation-time deduction of which branches within the `StandardRecord` object
need to be enabled when reading from the file.
Any unused branches are disabled.
For complicated `StandardRecord` objects, this can result in speedups of several orders of magnitude. 

## Usage
`SRProxy` needs to be templated over a concrete `StandardRecord` type that contains
the relevant fields for the user's needs.
In-practice examples include the implementations by [SBN](https://github.com/SBNSoftware/sbnana/tree/develop/sbnana/CAFAna)
and [DUNE](https://github.com/DUNE/lblpwgtools/tree/master/CAFAna).

It would be nice to have a technical digest of how to do this here, but in the meantime, 
please contact the [CAFAna librarian](https://github.com/orgs/cafana/teams/librarian)
and we can discuss your use case.
