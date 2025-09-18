#ifndef STRUCT_H
#define STRUCT_H

typedef struct PreviousSavings
{
    double previous_total_saving;
    double previous_travels;
    double previous_purchase;
    double previous_emergencies;
}
PreviousSavings;

typedef struct PreviousInvestment
{
    double previous_total_investment;
    double previous_real_estate;
    double previous_currencies;
    double previous_commodities;
    double previous_stocks;
}
PreviousInvestment;

typedef struct Savings
{
    double total_saving;
    double travels;
    double purchase;
    double emergencies;
}
Savings;

typedef struct Investment
{
    double investment;
    double real_estate;
    double currencies;
    double commodities;
    double stocks;
}
Investment;

typedef struct Items
{
    char name[51];
    double value;
    double percentage;
}
Items;
#endif