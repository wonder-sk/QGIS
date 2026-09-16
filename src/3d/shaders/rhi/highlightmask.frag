#version 440

// Marks every pixel a highlighted feature covers. The outline pass then draws a band
// around what this leaves behind.
//
// It shares shadowcaster.vert, because both passes want the same thing from a vertex:
// where it lands and nothing else.

layout( location = 0 ) out float coverage;

void main()
{
  coverage = 1.0;
}
