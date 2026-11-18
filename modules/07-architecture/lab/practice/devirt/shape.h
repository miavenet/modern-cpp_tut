// shape.h - interface used across a translation-unit boundary (commands in use.cpp).
#pragma once
struct Shape { virtual ~Shape() = default; virtual long area(long side) = 0; };
struct Square final : Shape { long acc = 0; long area(long side) override; };
Shape& make_shape();     // defined in shape.cpp
Square& make_square();   // defined in shape.cpp
