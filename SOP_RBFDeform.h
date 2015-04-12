


#ifndef __SOP_RBFDeform_h__
#define __SOP_RBFDeform_h__


#include <GA/GA_SplittableRange.h>
#include <GA/GA_Range.h>
#include <GA/GA_PageIterator.h>
#include <GA/GA_PageHandle.h>

#include <SOP/SOP_Node.h>
#include "interpolation.h"

#include <time.h>


#define ALGLIB_MODEL_QNN 0
#define ALGLIB_MODEL_ML   1

#define ALGLIB_TERM_LINEAR 0
#define ALGLIB_TERM_CONST  1
#define ALGLIB_TERM_ZERO   2

namespace RBFDeform {

class Timer 
{
    private:
        double begTime;
    public:
        void start()
        {
            begTime = clock();
        }

        double current() 
        {
        //int threads = UT_Thread::getNumProcessors();
            return (difftime(clock(), begTime) / CLOCKS_PER_SEC);// / (double) threads;
        }

        bool isTimeout(double seconds)
        {
            return seconds >= current();
        }
};



class SOP_RBFDeform : public SOP_Node
{
public:
         SOP_RBFDeform(OP_Network *net, const char *name, OP_Operator *op);
    virtual ~SOP_RBFDeform();

    static PRM_Template      myTemplateList[];
    static OP_Node      *myConstructor(OP_Network*, const char *,
                                OP_Operator *);

    /// This method is created so that it can be called by handles.  It only
    /// cooks the input group of this SOP.  The geometry in this group is
    /// the only geometry manipulated by this SOP.
    virtual OP_ERROR         cookInputGroups(OP_Context &context, 
                        int alone = 0);

protected:
    /// Method to cook geometry for the SOP
    virtual OP_ERROR         cookMySop(OP_Context &context);

private:
    void    getGroups(UT_String &str){ evalString(str, "group", 0, 0); }
    void    MODEL(UT_String &str)    { evalString(str, "model", 0, 0); }
    void    TERM(UT_String &str)     { evalString(str, "term", 0, 0); }
    fpreal  QCOEF(fpreal t)          { return evalFloat("qcoef", 0, t); }
    fpreal  ZCOEF(fpreal t)           { return evalFloat("zcoef", 0, t); }
    fpreal  RADIUS(fpreal t)    { return evalFloat("radius", 0, t); }
    int     LAYERS(fpreal t)    { return evalInt("layers", 0, t); }
    fpreal  LAMBDA(fpreal t)    { return evalFloat("lambda", 0, t); }

    /// This is the group of geometry to be manipulated by this SOP and cooked
    /// by the method "cookInputGroups".
    const GA_PointGroup *myGroup;
};

class op_RBFDeform {
public:
    op_RBFDeform(const std::string &str_model, GU_Detail *gdp)
        : mystr_model(str_model),  myGdp(gdp) {};
            // Take a SplittableRange (not a GA_Range)
    void    operator()(const GA_SplittableRange &r) const
            {
                alglib::rbfmodel model;
                GA_RWPageHandleV3 handle_P(myGdp->getP());
                // Execute storage:
                alglib::real_1d_array coord("[0,0,0]");
                alglib::real_1d_array result("[0,0,0]");
                std::string copy_serialized(mystr_model);
                alglib::rbfunserialize(copy_serialized, model);
                
                // Iterate over pages in the range
                for (GA_PageIterator pit = r.beginPages(); !pit.atEnd(); ++pit)
                {
                    GA_Offset start, end;
                    // iterate over the elements in the page.
                    for (GA_Iterator it(pit.begin()); it.blockAdvance(start, end); )
                    {
                        // Perform any per-page setup required, then
                        handle_P.setPage(start);
                        for (GA_Offset i = start; i < end; ++i)
                        {
                            UT_Vector3 pos = handle_P.get(i);
                            const double dp[] = {pos.x(), pos.y(), pos.z()};
                            coord.setcontent(3, dp);
                            alglib::rbfcalc(model, coord, result);
                            const UT_Vector3 delta = UT_Vector3(result[0], result[1],result[2]);
                            handle_P.set(i, pos+delta);
                        }
                    }
                }
            }
    private:
            const std::string &mystr_model;
            GU_Detail         *myGdp;
};
void
rbfDeformThreaded(const GA_Range &range, const std::string &str_model, GU_Detail *gdp)
{
    // Create a GA_SplittableRange from the original range
    GA_SplittableRange split_range = GA_SplittableRange(range);
    UTparallelFor(split_range, op_RBFDeform(str_model, gdp));
}




} // End HDK_Sample namespace

#endif
