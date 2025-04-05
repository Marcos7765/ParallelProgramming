#include <cstdio>
#include <iostream>
#include <unordered_map>
#include <string>
#include <cerrno>
#include "utils.h"
#include <mpreal.h>

#ifndef PRECISION
    #define PRECISION 32768 //2^15, 1000 terms candidate
#endif

void estimate_pi_chudnovsky(size_t n, mpfr::mpreal* real_out){
    *real_out = 0.;
    for (size_t k = 0; k < n; k++){
        #define rr(x) mpfr::mpreal(x, real_out->get_prec())
        #define factorial(x) mpfr::fac_ui(x, real_out->get_prec())

        //there probably are more casts than needed
        mpfr::mpreal temp = factorial(6*k)*(rr(545140134)*rr(k) + rr(13591409))/(
            mpfr::pow(factorial(k), 3)*factorial(3*k)*mpfr::pow(rr(640320), rr(3.*k) +1.5)
        );
        *real_out += (k%2 == 0) ? temp : -temp;
    }
    *real_out *= rr(12.);
    *real_out = rr(1.)/(*real_out);
}

//returns -7 if there's no error for the given precision
long get_pi_decimal_place_precision(mpfr::mpreal value){
    mpfr::mpreal err = mpfr::const_pi(value.get_prec()) - value;
    
    err = err > 0. ? err : -err;
    if (err == 0){return -7;}
    err = mpfr::log10(err);
    return err > 0. ? 0 : mpfr::ceil(-err).toLong() -1;
}

enum PRINT_MODE {
    REGULAR,
    CSV
};


std::unordered_map<std::string, PRINT_MODE> print_flags = {
    {"-printCSV", CSV},
};


int main(int argc, char* argv[]){
    PRINT_MODE print_mode = REGULAR;
    if (argc < 2){
        printf("Insufficient arguments, use: %s <NUM_ITER> [-printCSV]\n", argv[0]);
        exit(1);
    } else{
        if (argc > 3) {
            printf("Too many arguments starting at %s. Use: %s <NUM_ITER> [-printCSV]\n",
                argv[3], argv[0]);
            exit(1);
        }
        if (argc == 3){
            auto temp_print = print_flags.find(argv[2]);
            if (temp_print == print_flags.end()){
                printf("Invalid argument: %s. Usage: %s <NUM_ITER> [-printCSV]\n",
                    argv[3], argv[0]);
                exit(1);
            }
            print_mode = temp_print->second;
        }
    }

    size_t params[1];
    for (auto i = 0; i < 1; i++){
        params[i] = std::strtoul(argv[1+i], NULL, 10);
        if (params[i] == 0 || errno == ERANGE){
            printf("Invalid parameter: %s.\n", argv[1+i]);
            exit(1);
        }
    }

    auto n = params[0];
    mpfr::mpreal estimated_pi = mpfr::mpreal(0, PRECISION);
    auto start = utils::mark_time();
    estimate_pi_chudnovsky(n, &estimated_pi);
    auto end = utils::mark_time();
    double time_ms = utils::calc_time_interval_ms(start, end);
    mpfr::mpreal cmp_evo_pi = mpfr::mpreal(0, PRECISION);
    estimate_pi_chudnovsky(n-1, &cmp_evo_pi);
    mpfr::mpreal evolution = (estimated_pi - mpfr::const_pi(estimated_pi.get_prec())) - 
        (cmp_evo_pi - mpfr::const_pi(estimated_pi.get_prec()));
    
    long int display_evo_places = (evolution == 0) ? -7 : mpfr::log10(mpfr::abs(evolution)) > 0. ? 0 : mpfr::ceil(-mpfr::log10(mpfr::abs(evolution))).toLong() -1;
    long int decimal_places_precision = get_pi_decimal_place_precision(estimated_pi);

    //7 will be our error status to signal to run_tests that going further with this precision won't matter
    if (display_evo_places==-7 || decimal_places_precision ==-7){exit(7);}

    switch (print_mode)
    {
    case CSV:
        printf("%lu,%ld,%ld,%f\n", n, decimal_places_precision, display_evo_places, time_ms);
        break;
    default:
        printf("Result value: %s; in %f ms with an error around %s (%ld decimal places of precision); delta_error around %s\n", 
            estimated_pi.toString(decimal_places_precision).c_str(), time_ms, 
            (estimated_pi - mpfr::const_pi(estimated_pi.get_prec())).toString(decimal_places_precision > 6 ? 6 : decimal_places_precision).c_str(),
            decimal_places_precision, evolution.toString(2).c_str()
        );
        break;
    }
    return 0;
}