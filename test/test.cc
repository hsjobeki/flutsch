#include <catch2/catch_test_macros.hpp>
#include <flutsch.hh>

TEST_CASE( "Factorial of 0 is 1 (fail)", "[single-file]" ) {
    REQUIRE( 0 == 1 );
}

TEST_CASE( "Factorials of 1 and higher are computed (pass)", "[single-file]" ) {
    // REQUIRE( Factorial(1) == 1 );
    // REQUIRE( Factorial(2) == 2 );
    // REQUIRE( Factorial(3) == 6 );
    // REQUIRE( Factorial(10) == 3628800 );
}
