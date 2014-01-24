#ifndef MASS_SHIFT_DECORATOR
#define MASS_SHIFT_DECORATOR

#include "SpinorODE.h"
#include "Operator.h"

class MassShiftDecorator : public OneBodyOperatorDecorator, public SpinorODEDecorator
{
public:
    MassShiftDecorator(OneBodyOperator* wrapped_OBO, SpinorODE* wrapped_ODE, pOPIntegrator integration_strategy = pOPIntegrator());

    /** Set the inverse nuclear mass: 1/M. */
    void SetInverseMass(double InverseNuclearMass) { lambda = InverseNuclearMass; }
    double GetInverseMass() const { return lambda; }

public:
    /** Set exchange (nonlocal) potential and energy for ODE routines. */
    virtual void SetODEParameters(const SingleParticleWavefunction& approximation);
    
    /** Get exchange (nonlocal) potential. */
    virtual SpinorFunction GetExchange(pSingleParticleWavefunctionConst approximation) const;

    virtual void GetODEFunction(unsigned int latticepoint, const SpinorFunction& fg, double* w) const;
    virtual void GetODECoefficients(unsigned int latticepoint, const SpinorFunction& fg, double* w_f, double* w_g, double* w_const) const;
    virtual void GetODEJacobian(unsigned int latticepoint, const SpinorFunction& fg, double** jacobian, double* dwdr) const;

public:
    virtual SpinorFunction ApplyTo(const SpinorFunction& a) const;

protected:
    virtual SpinorFunction CalculateExtraExchange(const SpinorFunction& s) const;
    virtual double CalculateSMS(const SpinorFunction& s1, const SpinorFunction& s2, RadialFunction* p) const;

protected:
    double lambda;
    SpinorFunction extraExchangePotential;
};

#endif
