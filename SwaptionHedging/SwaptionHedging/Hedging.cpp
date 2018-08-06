#pragma warning (disable: 4819)
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
#include "CSVparser.hpp"

#include <fstream> 
#include <string>
#include <iostream>
#include <streambuf> 
#include <boost/timer.hpp>
#include <iostream>
#include <iomanip>

using namespace QuantLib;
using namespace std;

#if defined(QL_ENABLE_SESSIONS)
namespace QuantLib {

	Integer sessionId() { return 0; }

}
#endif

/* This file is used to calculate daily swaption and underying swap price, and delta value used for heding. */

// read discount factors from files like "DF_20080701.csv"
vector <double> DiscountFactorVec(const string &filename) {
	Parser data = Parser(filename); // from CSVParser
	vector <double> result;
	result.push_back(1.0); // QuantLib requires the first discount factor must be 1.0
	for (int i = 0; i < data.rowCount(); i++)
	result.push_back(stod(data[i]["Discount"])); 
	return result;
} 

// read implied volatilities surface from files like "IV_20080701.csv"
vector <double> ImpliedVolatilityVec(const string &filename) {
	Parser data = Parser(filename);
	vector <double> result;
	// considering the liquitity, only use swaptions with 1-10 Yr maturities and tenors
	for (int i = 4; i <= 13; i++) {
		for (int j = 1; j <= 10; j++)
			result.push_back(stod(data[i][j])/100);
	}
	return result;
} 

// used for returning two values from "calculate" function
struct Result {
	double swapNPV;
	double swaptionNPV;
};

// calculating swaption price and underying swap value
Result calculate(const string &dateString, Date todaysDate) {
	//Number of swaptions to be calibrated to...
	Size numRows = 10;
	Size numCols = 10;
	Integer swapLengths[] = { 1,2,3,4,5,6,7,8,9,10 }; // tenor

	// construct strings like "DF_20080701.csv"
	string DFString = "DF_";
	DFString += dateString;
	DFString += ".csv";
	string IVString = "IV_";
	IVString += dateString;
	IVString += ".csv";

	vector < DiscountFactor > dfs = DiscountFactorVec(DFString); // Discount Factor
	vector < Volatility > swaptionVols = ImpliedVolatilityVec(IVString);

	Calendar calendar = TARGET();
	Settings::instance().evaluationDate() = todaysDate;

	std::vector <Date > dates; // Dates of each discount factor point

	Calendar cal = TARGET();
	EURLibor1M libor;
	DayCounter dc = libor.dayCounter();

	dates.push_back(todaysDate); dates.push_back(todaysDate + 3 * Months);
	dates.push_back(todaysDate + 5 * Months); dates.push_back(todaysDate + 8 * Months);
	dates.push_back(todaysDate + 11 * Months); dates.push_back(todaysDate + 14 * Months);
	dates.push_back(todaysDate + 17 * Months); dates.push_back(todaysDate + 20 * Months);
	dates.push_back(todaysDate + 2 * Years); dates.push_back(todaysDate + 3 * Years);
	dates.push_back(todaysDate + 4 * Years); dates.push_back(todaysDate + 5 * Years);
	dates.push_back(todaysDate + 6 * Years); dates.push_back(todaysDate + 7 * Years);
	dates.push_back(todaysDate + 8 * Years); dates.push_back(todaysDate + 9 * Years);
	dates.push_back(todaysDate + 10 * Years); dates.push_back(todaysDate + 11 * Years);
	dates.push_back(todaysDate + 12 * Years); dates.push_back(todaysDate + 15 * Years);
	dates.push_back(todaysDate + 20 * Years); dates.push_back(todaysDate + 25 * Years);
	dates.push_back(todaysDate + 30 * Years); dates.push_back(todaysDate + 40 * Years);
	dates.push_back(todaysDate + 50 * Years);

	// build yield curve
	Handle<YieldTermStructure> rhTermStructure(
		boost::shared_ptr<InterpolatedDiscountCurve<Linear> >(
			new InterpolatedDiscountCurve < Linear >(dates, dfs, dc, cal)));

	// Define properties of swap/swaption
	Frequency fixedLegFrequency = Semiannual;
	BusinessDayConvention fixedLegConvention = Unadjusted;
	BusinessDayConvention floatingLegConvention = ModifiedFollowing;
	DayCounter fixedLegDayCounter = Thirty360(Thirty360::European);
	Frequency floatingLegFrequency = Quarterly;
	VanillaSwap::Type type = VanillaSwap::Payer;
	boost::shared_ptr<IborIndex> indexThreeMonths(new
		Euribor3M(rhTermStructure));

	// defining the swaptions to be used in model calibration
	std::vector<Period> swaptionMaturities;
	for (int i = 1; i <= 10; i++) {
		swaptionMaturities.push_back(Period(i, Years)); // swaption maturity
	}
	std::vector<boost::shared_ptr<CalibrationHelper> > swaptions;

	// define the 100 swaptions used for calibration
	Size i;
	for (i = 0; i<numRows; i++) {
		Size j = numCols - i - 1;
		Size k = i * numCols + j;
		boost::shared_ptr<Quote> vol(new SimpleQuote(swaptionVols[k]));
		swaptions.push_back(boost::shared_ptr<CalibrationHelper>(new
			SwaptionHelper(swaptionMaturities[i], 
				Period(swapLengths[j], Years),
				Handle<Quote>(vol), //const Handle<Quote>& volatility,
				indexThreeMonths, // boost::shared_ptr<IborIndex>& index
				indexThreeMonths->tenor(), //Period& fixedLegTenor,
				indexThreeMonths->dayCounter(), //const DayCounter& fixedLegDayCounter,
				indexThreeMonths->dayCounter(),
				rhTermStructure))); // const Handle<YieldTermStructure>& termStructure,
	} 

	// defining the models
	boost::shared_ptr<HullWhite> modelHW(new HullWhite(rhTermStructure)); 				

	// perform calibration
	for (i = 0; i < swaptions.size(); i++) {
		swaptions[i]->setPricingEngine(boost::shared_ptr<PricingEngine>(
			new JamshidianSwaptionEngine(modelHW)));
	}
	LevenbergMarquardt om;
	modelHW->calibrate(swaptions, om,
		EndCriteria(400, 100, 1.0e-8, 1.0e-8, 1.0e-8));

	/*  perform Swaption pricing  */
	// set settlement date and define the maturity and tenor of swaption
	Date settlement(01, July, 2008);
	Date startDate = calendar.advance(settlement, 7, Years,
		floatingLegConvention); // maturity
	Date maturity = calendar.advance(startDate, 6, Years,
		floatingLegConvention); // tenor
	Schedule fixedSchedule(startDate, maturity, Period(fixedLegFrequency),
		calendar, fixedLegConvention, fixedLegConvention,
		DateGeneration::Forward, false);
	Schedule floatSchedule(startDate, maturity, Period(floatingLegFrequency),
		calendar, floatingLegConvention, floatingLegConvention,
		DateGeneration::Forward, false);

	Rate FixedRate = 0.050826; // strike

	// define underying swap
	boost::shared_ptr<VanillaSwap> swap(new VanillaSwap(
		type, 1000.0,
		fixedSchedule, FixedRate, fixedLegDayCounter,
		floatSchedule, indexThreeMonths, 0.0,
		indexThreeMonths->dayCounter()));

	// price underying swap
	swap->setPricingEngine(boost::shared_ptr<PricingEngine>(
		new DiscountingSwapEngine(rhTermStructure)));

	// define swaption based on underying swap
	boost::shared_ptr<Exercise> europeanExercise(
		new EuropeanExercise(startDate));
	Swaption atmEuropeanSwaption(swap, europeanExercise);

	// price swaption
	atmEuropeanSwaption.setPricingEngine(boost::shared_ptr<PricingEngine>(
		new JamshidianSwaptionEngine(modelHW)));
	
	// calculating delta in black-76 based on swaption price
	Real IV = atmEuropeanSwaption.impliedVolatility(atmEuropeanSwaption.NPV(), rhTermStructure, 0.05);
	cout << "Start calculating delta." << endl;
	atmEuropeanSwaption.setPricingEngine(boost::shared_ptr<PricingEngine>(
		new BlackSwaptionEngine(rhTermStructure, IV)));
	std::cout << "Black Price :      " << atmEuropeanSwaption.NPV() << std::endl;
	cout << "End calculating delta." << endl;

	// return results
	struct Result ret;
	ret.swapNPV = swap->NPV();
	ret.swaptionNPV = atmEuropeanSwaption.NPV();
	return ret;
}

int main() {
	Date todaysDate(01, July, 2008);
	char digits_m[] = { '7', '8', '9', '0', '1', '2'};
	char digits_d[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
	char mddd[] = "20080701";
	int count = 0;

	struct Result r = calculate(string(mddd),todaysDate);
	cout << "Swap Value = " << r.swapNPV << endl;
	cout << "Swaption Value = " << r.swaptionNPV << endl <<endl;

	ofstream oFile;
	oFile.open("result7x6_2008.csv", ios::out | ios::trunc);
	oFile << "Date" << "," << "Swap Value" << "," << "Swaption Value" << endl;

		for (int mi = 0; mi < 6; mi++) { 
			char m = digits_m[mi]; // units digits of month, 7,8,9,0,1,2
			for (char dd = '0'; dd <= '3'; dd++) { // tens digits of day, 0,1,2,3
				for (int di = 0; di < 10; di++) { 
					char d = digits_d[di]; // units digits of day, 0 - 9
					mddd[5] = m;
					mddd[6] = dd;
					mddd[7] = d;
					if (dd == '0' && d == '0') continue; // skip "20xxxx00"
					if (dd == '3' && d == '2') break; // break at "20xxxx31"
					if (string(mddd) == "20080931") break; // skip "20xx0931"
					if (string(mddd) == "20081131") break; // skip "20xx1131"
					if (m == '0') mddd[4] = '1'; // adjust month when it's October

					cout << mddd << endl;
					cout << todaysDate << endl;

					struct Result r = calculate(string(mddd),todaysDate); // perform calculation
					cout << "Swap Value = " << r.swapNPV << endl;
					cout << "Swaption Value = " << r.swaptionNPV << endl <<endl;

					oFile << string(mddd) << "," << r.swapNPV << "," << r.swaptionNPV << endl;
					todaysDate += 1 * Days;
				}
			}
		}
	oFile.close();


	system("pause");
	return 0;

}

