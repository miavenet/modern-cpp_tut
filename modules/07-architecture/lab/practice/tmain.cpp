// tmain.cpp - driver so the tsize.cpp object file can be linked into a binary (commands in tsize.cpp).
long entry(int);
int main(int c, char**) { return (int)entry(c); }
