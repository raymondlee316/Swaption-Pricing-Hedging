/*
This file is used to price 100 Swaptions with maturities and tenors of 1-10 years based on Jamshdian model of Hull-White One
Factor model at 2008/07/01. The file first calibrate the Hull-White model based on market implied volatilities of 100 swaptions with 
yield curve it built, then price those swaption with the model. 
*/

#pragma warning off (disable: 4819)
#include <ql/qldefines.hpp>
#ifdef BOOST_MSVC
#  include <ql/auto_link.hpp>
#endif
#include <ql/quantlib.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/treeswaptionengine.hpp>
#include <ql/pricingengines/swaption/jamshidianswaptionengine.hpp>
#include <ql/pricingengines/swaption/g2swaptionengine.hpp>
#include <ql/pricingengines/swaption/fdhullwhiteswaptionengine.hpp>
#include <ql/pricingengines/swaption/fdg2swaptionengine.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/models/shortrate/onefactormodels/blackkarasinski.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <fstream> 
#include <string>
#include <iostream>
#include <streambuf> 
#include <boost/timer.hpp>
#include <iomanip>

using namespace QuantLib;
using namespace std;
ofstream oFile;

#if defined(QL_ENABLE_SESSIONS)
namespace QuantLib {

	Integer sessionId() { return 0; }

}
#endif


//Number of swaptions to be calibrated to...

Size numRows = 10;
Size numCols = 10;

Integer swapLengths[] = { 1,2,3,4,5,6,7,8,9,10 }; // tenor

Volatility swaptionVols[] = {
	0.719,0.598,0.5,0.428,0.3878,0.353,0.3297,0.3155,0.3048,0.2915,
	0.5442,0.4475,0.3835,0.3453,0.3215,0.3033,0.2888,0.281,0.273,0.2643,
	0.3808,0.3358,0.3075,0.29,0.2783,0.2688,0.2595,0.2577,0.252,0.2448,
	0.2965,0.2788,0.2655,0.2573,0.25,0.2442,0.2385,0.2375,0.2348,0.2288,
	0.258,0.252,0.2455,0.237,0.233,0.2285,0.2242,0.224,0.2215,0.217,
	0.2385,0.234,0.229,0.225,0.222,0.2188,0.2148,0.2135,0.2125,0.2095,
	0.225,0.2227,0.217,0.2125,0.208,0.208,0.204,0.2055,0.205,0.2008,
	0.212,0.211,0.2073,0.2035,0.2018,0.2005,0.198,0.198,0.1977,0.197,
	0.2045,0.2023,0.2002,0.1968,0.1943,0.193,0.1905,0.1902,0.1905,0.1908,
	0.1968,0.193,0.1907,0.1885,0.1868,0.186,0.1848,0.1865,0.1865,0.184 };  // swaption volatility surface at 2008/07/01

// use the following function to perform calibration based on all of the swaption
void calibrateModel(
	const boost::shared_ptr<ShortRateModel>& model,
	std::vector<boost::shared_ptr<CalibrationHelper> >& helpers) {

	LevenbergMarquardt om;
	model->calibrate(helpers, om,
		EndCriteria(400, 100, 1.0e-8, 1.0e-8, 1.0e-8));

	oFile.open("Calibration.csv", ios::out | ios::trunc);
	oFile << "Swaption" << "," << "Relative Difference of IV" << "," << "Relative Difference of Price" << endl;
	// Output part of the implied Black volatilities
	Size k = 0;
	for (Size i = 1; i <= numRows; i++) {
		for (Size j = 1; j <= numCols; j++) {
			Real npv = helpers[k]->modelValue();
			Volatility implied = helpers[k]->impliedVolatility(npv, 1e-4,
				1000, 0.001, 4.0);
			Real ModelValue = helpers[k]->blackPrice(implied);
			Real MarketValue = helpers[k]->marketValue();
			Volatility diff = implied - swaptionVols[k];
			//cout << i << "x" << j << endl;
			//cout << abs((ModelValue - MarketValue) / MarketValue) << endl;
			oFile << i << "x" << j << "," << abs(diff/ swaptionVols[k])<<","<< abs((ModelValue - MarketValue) / MarketValue) << endl;
			k++;
		}
	}
	oFile.close();
}

int main(int, char*[]) {
	Date todaysDate(01, July, 2008);
	Calendar calendar = TARGET();
	Settings::instance().evaluationDate() = todaysDate;

	std::vector <Date > dates; // Dates of each discount factor point
	std::vector < DiscountFactor > dfs; // Discount Factor
	Calendar cal = TARGET();
	EURLibor3M libor; // only for day count
	DayCounter dc = libor.dayCounter();
	Natural settlementDays = 0;
	Date settlement = cal.advance(todaysDate, settlementDays, Days);

	dates.push_back(settlement); dates.push_back(settlement + 3 * Months);
	dates.push_back(settlement + 5 * Months); dates.push_back(settlement + 8 * Months);
	dates.push_back(settlement + 11 * Months); dates.push_back(settlement + 14 * Months);
	dates.push_back(settlement + 17 * Months); dates.push_back(settlement + 20 * Months);
	dates.push_back(settlement + 2 * Years); dates.push_back(settlement + 3 * Years);
	dates.push_back(settlement + 4 * Years); dates.push_back(settlement + 5 * Years);
	dates.push_back(settlement + 6 * Years); dates.push_back(settlement + 7 * Years);
	dates.push_back(settlement + 8 * Years); dates.push_back(settlement + 9 * Years);
	dates.push_back(settlement + 10 * Years); dates.push_back(settlement + 11 * Years);
	dates.push_back(settlement + 12 * Years); dates.push_back(settlement + 15 * Years);
	dates.push_back(settlement + 20 * Years); dates.push_back(settlement + 25 * Years);
	dates.push_back(settlement + 30 * Years); dates.push_back(settlement + 40 * Years);

	// input disocunt factors at 2008/07/01
	dfs.push_back(1.0); dfs.push_back(0.999372);
	dfs.push_back(0.998612); dfs.push_back(0.997533);
	dfs.push_back(0.99626); dfs.push_back(0.994644);
	dfs.push_back(0.992501); dfs.push_back(0.989723);
	dfs.push_back(0.98572); dfs.push_back(0.96545);
	dfs.push_back(0.936598); dfs.push_back(0.90133);
	dfs.push_back(0.862887); dfs.push_back(0.82346);
	dfs.push_back(0.784508); dfs.push_back(0.746472);
	dfs.push_back(0.709697); dfs.push_back(0.674411);
	dfs.push_back(0.640635); dfs.push_back(0.549721);
	dfs.push_back(0.432787); dfs.push_back(0.343264);
	dfs.push_back(0.273871); dfs.push_back(0.180376);

	// build yield curve
	Handle<YieldTermStructure> rhTermStructure(
		boost::shared_ptr<InterpolatedDiscountCurve<Linear> >(
			new InterpolatedDiscountCurve <Linear>(dates, dfs, dc, cal)));

	// Define properties of swap/swaption
	Frequency fixedLegFrequency = Semiannual;
	BusinessDayConvention fixedLegConvention = Unadjusted;
	BusinessDayConvention floatingLegConvention = ModifiedFollowing;
	DayCounter fixedLegDayCounter = Thirty360(Thirty360::European);
	Frequency floatingLegFrequency = Quarterly;
	VanillaSwap::Type type = VanillaSwap::Payer;
	Rate dummyFixedRate = 0.03;
	boost::shared_ptr<IborIndex> indexSixMonths(new
		USDLibor(Period(3, Months), rhTermStructure));
			
	// defining the swaptions to be used in model calibration
	std::vector<Period> swaptionMaturities;
	for (int i = 1; i <= 10; i++) {
		swaptionMaturities.push_back(Period(i, Years)); // swaption maturity
	}
	std::vector<boost::shared_ptr<CalibrationHelper> > swaptions; // used for calibration
	Size i, j, k = 0;
	for (i = 1; i <= numRows; i++) {
		for (j = 1; j <= numCols; j++) {
			boost::shared_ptr<Quote> vol(new SimpleQuote(swaptionVols[k]));
			swaptions.push_back(boost::shared_ptr<CalibrationHelper>(new
				SwaptionHelper(swaptionMaturities[i - 1],  // maturity
					Period(swapLengths[j - 1], Years),  // tenor
					Handle<Quote>(vol), //const Handle<Quote>& volatility,
					indexSixMonths, // boost::shared_ptr<IborIndex>& index
					indexSixMonths->tenor(), //Period& fixedLegTenor,
					indexSixMonths->dayCounter(), //const DayCounter& fixedLegDayCounter,
					indexSixMonths->dayCounter(), // const DayCounter& floatingLegDayCounter,
					rhTermStructure))); // const Handle<YieldTermStructure>& termStructure,
			k++;
		}
	} 

	// defining the models
	boost::shared_ptr<HullWhite> modelHW(new HullWhite(rhTermStructure));
	for (i = 0; i < swaptions.size(); i++) {
		swaptions[i]->setPricingEngine(boost::shared_ptr<PricingEngine>(
			new JamshidianSwaptionEngine(modelHW)));
	}

	// calibrate based on all of swaptions in order to find the errors
	calibrateModel(modelHW, swaptions);

	std::cout << "calibrated to:\n"
		<< "a = " << modelHW->params()[0] << ", "
		<< "sigma = " << modelHW->params()[1]
		<< std::endl << std::endl;

	////////////////// Swaption Pricing /////////////////////////
	oFile.open("result.csv", ios::out | ios::trunc);
	for (int i = 1; i <= 10; i++) { // i for maturity
		for (int j = 1; j <= 10; j++) { // j for tenor
			Date startDate = calendar.advance(settlement, i, Years,
				floatingLegConvention);
			Date maturity = calendar.advance(startDate, j, Years,
				floatingLegConvention);
			Schedule fixedSchedule(startDate, maturity, Period(fixedLegFrequency),
				calendar, fixedLegConvention, fixedLegConvention,
				DateGeneration::Forward, false);
			Schedule floatSchedule(startDate, maturity, Period(floatingLegFrequency),
				calendar, floatingLegConvention, floatingLegConvention,
				DateGeneration::Forward, false);

			// define dummy swap in order to find ATM strike
			boost::shared_ptr<VanillaSwap> swap(new VanillaSwap(
				type, 1000.0,
				fixedSchedule, dummyFixedRate, fixedLegDayCounter,
				floatSchedule, indexSixMonths, 0.0,
				indexSixMonths->dayCounter())); // used for calculating fixedATMRate
			swap->setPricingEngine(boost::shared_ptr<PricingEngine>(
				new DiscountingSwapEngine(rhTermStructure))); // swap pricing
			Rate fixedATMRate = swap->fairRate();

			// define the underying swap of the swaption we want to price
			boost::shared_ptr<VanillaSwap> atmSwap(new VanillaSwap(
				type, 1.0, // type, nominal
				fixedSchedule, fixedATMRate, fixedLegDayCounter, // fixedSchedule, fixedRate, fixedDayCount
				floatSchedule, indexSixMonths, 0.0, // floatSchedule, iborIndex, spread
				indexSixMonths->dayCounter())); //floatingDayCount
			boost::shared_ptr<Exercise> europeanExercise(
				new EuropeanExercise(startDate));

			// define the swaption based on the underying swap
			Swaption atmEuropeanSwaption(atmSwap, europeanExercise);

			// perform pricing
			atmEuropeanSwaption.setPricingEngine(boost::shared_ptr<PricingEngine>(
				new JamshidianSwaptionEngine(modelHW)));
			oFile << i << "x" << j << "," << atmEuropeanSwaption.NPV() << ","
				<< atmEuropeanSwaption.impliedVolatility(atmEuropeanSwaption.NPV(), rhTermStructure, 0.05) << endl;
			//std::cout << /*"HW (Jamshidian) :      " << */atmEuropeanSwaption.NPV() << std::endl;
			//cout << /*"implied volatility:      " << */atmEuropeanSwaption.impliedVolatility(atmEuropeanSwaption.NPV(), rhTermStructure, 0.05) << endl;
		}
	}
	oFile.close();
	return 0;
}

